import sys
import os
import re
import jinja2
import interface_pb2
import operator
from google.protobuf.json_format import Parse, MessageToJson
from functools import reduce

from pprint import pprint

import hashlib

class Offset(int):
    def __new__(self, offset):
        assert isinstance(offset, int)
        return int.__new__(Offset, offset)

    def __add__(self, x):
        assert isinstance(x, int)
        new_offset = self.offset + x
        if self.offset < 0 and new_offset >= 0:
            new_offset = new_offset + 1
        elif self.offset > 0 and new_offset <= 0:
            new_offset = new_offset - 1
        return Offset(new_offset)

    @property
    def offset(self):
        return int(self)

    def __sub__(self, x):
        return self + (-x)

class Level(tuple):
    def __new__(self, splitter, offset):
        assert isinstance(splitter, int)
        assert isinstance(offset, Offset)

        return tuple.__new__(Level, (splitter, offset))

    def __add__(self, x):
        assert isinstance(x, tuple) and len(x) == 2
        return Level(self.splitter + x[0], self.offset + x[1])

    @property
    def splitter(self):
        return self[0]

    @property
    def offset(self):
        return self[1]

class Interval(tuple):
    def __new__(self, begin, end):
        assert isinstance(begin, Level)
        assert isinstance(end, Level)

        return tuple.__new__(Interval, (begin, end))

    @property
    def begin(self):
        return self[0]

    @property
    def end(self):
       return self[1]

    def contains(self, level):
        assert isinstance(level, Level)
        return self.begin <= level and self.end >= level

def message_to_level(l):
    return Level(l.splitter, Offset(l.offset))
def message_to_interval(i):
    return Interval(message_to_level(i.begin), message_to_level(i.end))

in_file = sys.argv[1]
out_file = sys.argv[2]
h = hashlib.blake2b(sys.argv[1].encode('utf-8'), digest_size=7)

computation = interface_pb2.Computation()
with open(in_file, "rb") as f:
    computation.ParseFromString(f.read())
#  print(MessageToJson(computation, including_default_value_fields=True))

mss1 = computation.multistages[0]

stage_extents = []
arg_extents = dict()

def max_extent(extent1, extent2):
    return (
        min(extent1[0], extent2[0]),
        max(extent1[1], extent2[1]),
        min(extent1[2], extent2[2]),
        max(extent1[3], extent2[3])
        )
def add_extent(extent1, extent2):
    return list(map(operator.add, extent1, extent2))

def merge_dicts(f, A, B):
    return {k: f(A[k], B[k]) if k in A and k in B else A.get(k, B.get(k)) \
        for k in A.keys() | B.keys()}

stage_analysis_data = []
for mss in reversed(computation.multistages):
    mss_data = []
    for ds in reversed(mss.dependent_stages):
        ds_data = []
        new_arg_extents = dict()
        for stage_ref in ds.independent_stages:
            stage = computation.stages[stage_ref.name]
            stage_extent = (0, 0, 0, 0)
            for arg, accessor in zip(stage_ref.args, stage.accessors):
                if accessor.HasField("normal_accessor") \
                    and accessor.normal_accessor.intent == interface_pb2.NormalAccessor.READ_WRITE:

                    arg_extent = arg_extents[(arg.id, arg.arg_type)] \
                            if (arg.id, arg.arg_type) in arg_extents else (0, 0, 0, 0)

                    stage_extent = max_extent(stage_extent, arg_extent)
            ds_data.append(stage_extent)

            for arg, accessor in zip(stage_ref.args, stage.accessors):
                if accessor.HasField("normal_accessor"):
                    stage_arg_extent = add_extent(stage_extent, (
                        accessor.normal_accessor.extent.iminus,
                        accessor.normal_accessor.extent.iplus,
                        accessor.normal_accessor.extent.jminus,
                        accessor.normal_accessor.extent.jplus))

                    new_arg_extents[(arg.id, arg.arg_type)] = \
                        max_extent(stage_arg_extent, new_arg_extents[(arg.id, arg.arg_type)]) \
                            if (arg.id, arg.arg_type) in new_arg_extents else stage_arg_extent

        arg_extents = merge_dicts(max_extent, new_arg_extents, arg_extents)

        mss_data.append(ds_data)
    stage_analysis_data.append(list(reversed(mss_data)))
stage_analysis_data = list(reversed(stage_analysis_data))

multistages = []
for mss_id, (mss, mss_stage_analysis) in enumerate(zip(computation.multistages, stage_analysis_data)):
    stages = [stage.name for d in mss.dependent_stages for stage in d.independent_stages]
    intervals = [message_to_interval(interval.interval) for stage in stages for interval in computation.stages[stage].intervals]
    levels = set([i.begin for i in intervals] + [i.end + (0, 1) for i in intervals])
    levels = list(sorted(list(levels)))

    mss_data = {}
    mss_data["intervals"] = []
    mss_data["id"] = mss_id
    for begin, end in zip(levels[:-1], levels[1:]):
        first = begin
        last = end + (0, -1)

        i_data = []

        stage_id = 0

        for d in mss.dependent_stages:
            d_data = []
            for stage_ref in d.independent_stages:
                for i in computation.stages[stage_ref.name].intervals:
                    interval = message_to_interval(i.interval)
                    if interval.contains(first):
                        overload = interval if i.overload == interface_pb2.StageInterval.INTERVAL else "none"
                        d_data.append({"name": stage_ref.name, "id": stage_id, "overload": overload})
                stage_id = stage_id + 1
            i_data.append(d_data)
        mss_data["intervals"].append({"interval": (first, last), "stages": i_data})

    mss_data["stages"] = []
    stage_id = 0

    k_extents = {}
    for d, d_stage_extents in zip(mss.dependent_stages, mss_stage_analysis):
        d_data = []
        for stage_ref, stage_extent in zip(d.independent_stages, d_stage_extents):
            s_data = {}

            s_data["stage_extent"] = stage_extent
            s_data["name"] = stage_ref.name
            s_data["id"] = stage_id

            s_data["argmap"] = []
            stage = computation.stages[stage_ref.name]
            for arg, accessor in zip(stage_ref.args, stage.accessors):
                accessor_id = accessor.normal_accessor.id if accessor.HasField("normal_accessor") else \
                    accessor.global_accessor.id
                if arg.arg_type == interface_pb2.Multistage.NORMAL:
                    arg_type = "normal"
                elif arg.arg_type == interface_pb2.Multistage.GLOBAL:
                    arg_type = "global"
                else:
                    arg_type = "temporary"
                s_data["argmap"].append({"arg": (arg_type, arg.id), "accessor": accessor_id})

                if accessor.HasField("normal_accessor"):
                    if not (arg.arg_type, arg.id) in k_extents:
                        k_extents[(arg.arg_type, arg.id)] = (accessor.normal_accessor.extent.kminus,
                            accessor.normal_accessor.extent.kplus)
                    else:
                        old = k_extents[(arg.arg_type, arg.id)]
                        k_extents[(arg.arg_type, arg.id)] = (
                            min(old[0], accessor.normal_accessor.extent.kminus),
                            max(old[1], accessor.normal_accessor.extent.kplus))

            d_data.append(s_data)

            stage_id = stage_id + 1
        mss_data["stages"].append(d_data)

    max_stage_extent = reduce(lambda a, b: max_extent(a, reduce(max_extent, b)), \
        mss_stage_analysis, (0, 0, 0, 0))
    mss_data["max_stage_extent"] = max_stage_extent

    mss_data["kinds"] = {}
    mss_data["direction"] = "forward" if mss.policy == interface_pb2.Multistage.FORWARD else "backward" if mss.policy == interface_pb2.Multistage.BACKWARD else "parallel"
    mss_data["temporaries"] = {}
    mss_data["temporaries"]["args"] = {}
    mss_data["globals"] = {}
    args = set()
    for d in mss.dependent_stages:
        for stage_ref in d.independent_stages:
            stage = computation.stages[stage_ref.name]
            for arg_ref, accessor in zip(stage_ref.args, stage.accessors):
                if arg_ref.arg_type == interface_pb2.Multistage.TEMPORARY:
                    if not arg_ref.id in mss_data["temporaries"]["args"]:
                        mss_data["temporaries"]["args"][arg_ref.id] = {
                            "type": computation.temporaries[arg_ref.id].type
                        }

                    if not "readonly" in mss_data["temporaries"]["args"][arg_ref.id] or \
                        mss_data["temporaries"]["args"][arg_ref.id]["readonly"]:

                        mss_data["temporaries"]["args"][arg_ref.id]["readonly"] = \
                            (accessor.normal_accessor.intent == interface_pb2.NormalAccessor.READ_ONLY)

                elif arg_ref.arg_type == interface_pb2.Multistage.NORMAL:
                    arg = computation.fields.args[arg_ref.id]
                    kind = computation.fields.kinds[arg.kind]

                    if not arg.kind in mss_data["kinds"]:
                        mss_data["kinds"][arg.kind] = {}
                        mss_data["kinds"][arg.kind]["args"] = {}

                    if not arg_ref.id in mss_data["kinds"][arg.kind]["args"]:
                        mss_data["kinds"][arg.kind]["args"][arg_ref.id] = {}
                        mss_data["kinds"][arg.kind]["args"][arg_ref.id]["type"] = arg.type

                    if not "readonly" in mss_data["kinds"][arg.kind]["args"][arg_ref.id] or \
                        mss_data["kinds"][arg.kind]["args"][arg_ref.id]["readonly"]:

                        mss_data["kinds"][arg.kind]["args"][arg_ref.id]["readonly"] = \
                            (accessor.normal_accessor.intent == interface_pb2.NormalAccessor.READ_ONLY)
                else:
                    arg = computation.global_params[arg_ref.id]
                    mss_data["globals"][arg_ref.id] = {
                        "type": arg.type
                    }


            args = args | set([(arg.arg_type, arg.id) for arg in stage_ref.args])

    mss_data["k_caches"] = []
    for k_cache in mss.k_caches:
        mss_data["k_caches"].append({
            "id": k_cache.id,
            "temporary": k_cache.temporary,
            "fill": k_cache.fill,
            "flush": k_cache.flush,
            "type": k_cache.type,
            "min_extent": k_extents[(k_cache.temporary, k_cache.id)][0],
            "max_extent": k_extents[(k_cache.temporary, k_cache.id)][1],
        })

        if not k_cache.temporary:
            mss_data["k_caches"][-1]["kind"] = computation.fields.args[k_cache.id].kind
            if k_cache.fill or k_cache.flush:
                mss_data["k_caches"][-1]["readonly"] = mss_data["kinds"][computation.fields.args[k_cache.id].kind]["args"][k_cache.id]["readonly"]
            mss_data["kinds"][computation.fields.args[k_cache.id].kind]["args"][k_cache.id]["cached"] = "K"
            mss_data["kinds"][computation.fields.args[k_cache.id].kind]["args"][k_cache.id]["local"] = not k_cache.fill and not k_cache.flush
        else:
            if k_cache.fill or k_cache.flush:
                mss_data["k_caches"][-1]["readonly"] = mss_data["temporaries"]["args"][k_cache.id]["readonly"]

            mss_data["temporaries"]["args"][k_cache.id]["cached"] = "K"
            mss_data["temporaries"]["args"][k_cache.id]["local"] = not k_cache.fill and not k_cache.flush

    mss_data["ij_caches"] = []
    for ij_cache in mss.ij_caches:
        mss_data["ij_caches"].append({
            "id": ij_cache.id,
            "temporary": ij_cache.temporary,
            "type": ij_cache.type,
        })

        if not ij_cache.temporary:
            mss_data["kinds"][computation.fields.args[ij_cache.id].kind]["args"][ij_cache.id]["cached"] = "K"
        else:
            mss_data["temporaries"]["args"][ij_cache.id]["cached"] = "K"

    multistages.append(mss_data)


#start setting up dict...
context = dict()
max_arg_extent = reduce(max_extent, arg_extents.values())
context["max_arg_extent"] = max_arg_extent
context["offset_limit"] = computation.offset_limit

# kinds
context["kinds"] = {}
for kind_id, kind_info in computation.fields.kinds.items():
    context["kinds"][kind_id] = {}
    context["kinds"][kind_id]["layout"] = list(kind_info.layout)
    context["kinds"][kind_id]["args"] = []

# args
context["args"] = {}
for arg_id, arg_info in computation.fields.args.items():
    context["args"][arg_id] = {
        "kind": arg_info.kind,
        "type": arg_info.type,
        "readonly": False, # TODO
    }
    context["kinds"][arg_info.kind]["args"].append(arg_id)

context["temporaries"] = {}
for field_id, field_info in computation.temporaries.items():
    context["temporaries"][field_id] = {}
    context["temporaries"][field_id]["selector"] = list(field_info.selector)
    context["temporaries"][field_id]["type"] = field_info.type

context["multistages"] = multistages
context["hash"] = int(h.hexdigest(), 16)
context["name"] = sys.argv[1].split("__")[-1]

pprint(context)


env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(os.path.join(os.path.dirname(__file__), "./templates")),
    autoescape=False,
    extensions = ['jinja2.ext.do']
)
env.globals['merge_dict'] = lambda *args, **kwargs: {key: value for arg in [*args, kwargs] for key, value in arg.items()}
class JoinerFirst(object):
    def __init__(self, sep=u', ', first=u''):
        self.first= first
        self.sep = sep
        self.used = False

    def __call__(self):
        if not self.used:
            self.used = True
            return self.first
        return self.sep
env.globals['joiner_first'] = JoinerFirst
env.filters['dictsort_by_value'] = lambda d, *, attr: collections.OrderedDict(sorted(d.items(), key=lambda t: t[1][attr])).items()
env.filters['kind_dimension'] = lambda k: sum(1 for v in (k['layout'] if 'layout' in k else k['selector']) if v != -1)
env.filters['masked'] = lambda k, i: k['layout'][i] == -1 if 'layout' in k else k['selector'][i]
env.filters['unitstride'] = lambda k, i: k['layout'][i] == 0 if 'layout' in k else not any(k['selector'][0:i])
env.filters['has_kcaches'] = lambda multistage: 'k_caches' in multistage.keys() and len(multistage['k_caches']) > 0
env.filters['to_identifier'] = lambda name: re.sub(r'[()<>:]', "_", name)
env.filters['bool_to_str'] = lambda b: 'true' if b else 'false'

template = env.get_template("generator.cpp.j2")

with open(sys.argv[2], "w") as out_f:
    print(template.render(context=context), end='', file=out_f)

os.system("clang-format -i {}".format(sys.argv[2]))

#  print(MessageToJson(computation, including_default_value_fields=True))



    #  with open(sys.argv[1]) as in_f:
        #  for line in in_f:
            #  if "GT_DUMP_GENERATED_CODE" in line:
                #  ret = re.findall(r'(GT_DUMP_GENERATED_CODE\((.*)\))', line)[0]
                #  h = hashlib.blake2b(ret[1].encode('utf-8'), digest_size=7)
                #  newtext = """
                    #  // this code was generated
                    #  namespace gridtools {
                        #  template <>
                        #  struct generated_computation<std::integral_constant<long int, %s>> {
                            #  void run() { std::cout << "run" << std::endl; }
                            #  void reset_meter() {}
                            #  std::string print_meter() const { return ""; }
                        #  };
                    #  } // namespace gridtools
                    #  """ % str(int(h.hexdigest(), 16))
                #  line = line.replace(ret[0], newtext)
                #  print(line, end='', file=out_f)

            #  elif "GT_DUMP_IDENTIFIER" in line:
                #  ret = re.findall(r'(GT_DUMP_IDENTIFIER\((.*)\))', line)[0]
                #  h = hashlib.blake2b(ret[1].encode('utf-8'), digest_size=7)
                #  #  line = line.replace(ret[0], str(hash(ret[1])))
                #  line = line.replace(ret[0], str(int(h.hexdigest(), 16)))
                #  print(line, end='', file=out_f)
            #  else:
                #  print(line, end='', file=out_f)

#  os.rename(sys.argv[1] + ".tmp", sys.argv[1])

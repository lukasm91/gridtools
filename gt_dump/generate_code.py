import sys
import os
import re
import jinja2
import interface_pb2
import operator
from itertools import groupby
from google.protobuf.json_format import Parse, MessageToJson
from functools import reduce
import collections

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


# replaces some chars that are not supported in identifier
def to_identifier(name):
    return re.sub(r"[()<>:, ]", "_", name)


def merge_dicts(f, A, B):
    return {
        k: f(A[k], B[k]) if k in A and k in B else A.get(k, B.get(k))
        for k in A.keys() | B.keys()
    }


def max_extent(extent1, extent2):
    return (
        min(extent1[0], extent2[0]),
        max(extent1[1], extent2[1]),
        min(extent1[2], extent2[2]),
        max(extent1[3], extent2[3]),
    )


def add_extent(extent1, extent2):
    return tuple(map(operator.add, extent1, extent2))


class Generator:
    def __init__(self, in_file):
        self.computation = interface_pb2.Computation()
        with open(in_file, "rb") as f:
            self.computation.ParseFromString(f.read())

        self.computation_id = int(
            hashlib.blake2b(in_file.encode("utf-8"), digest_size=7).hexdigest(), 16
        )
        self.computation_name = sys.argv[1].split("__")[-1]

    def dump_input(self):
        print(MessageToJson(self.computation, including_default_value_fields=True))

    def _stage_analysis(self):
        arg_extents = dict()
        stage_analysis_data = []
        for mss in reversed(self.computation.multistages):
            mss_data = []
            for ds in reversed(mss.dependent_stages):
                ds_data = []
                new_arg_extents = dict()
                for stage_ref in ds.independent_stages:
                    stage = self.computation.stages[stage_ref.name]
                    stage_extent = (0, 0, 0, 0)
                    for arg, accessor in zip(stage_ref.args, stage.accessors):
                        if accessor.intent == interface_pb2.Accessor.READ_WRITE:

                            arg_extent = (
                                arg_extents[(arg.id, arg.arg_type)]
                                if (arg.id, arg.arg_type) in arg_extents
                                else (0, 0, 0, 0)
                            )

                            stage_extent = max_extent(stage_extent, arg_extent)
                    ds_data.append(stage_extent)

                    for arg, accessor in zip(stage_ref.args, stage.accessors):
                        stage_arg_extent = add_extent(
                            stage_extent,
                            (
                                accessor.extent.iminus,
                                accessor.extent.iplus,
                                accessor.extent.jminus,
                                accessor.extent.jplus,
                            ),
                        )

                        new_arg_extents[(arg.id, arg.arg_type)] = (
                            max_extent(
                                stage_arg_extent,
                                new_arg_extents[(arg.id, arg.arg_type)],
                            )
                            if (arg.id, arg.arg_type) in new_arg_extents
                            else stage_arg_extent
                        )

                arg_extents = merge_dicts(max_extent, new_arg_extents, arg_extents)

                mss_data.append(ds_data)
            stage_analysis_data.append(list(reversed(mss_data)))
        stage_analysis_data = list(reversed(stage_analysis_data))

        return stage_analysis_data, arg_extents

    @staticmethod
    def _restore_stage_name(name):
        return name.replace("(anonymous namespace)::", "")

    @staticmethod
    def _make_stage_id(id_d, id_i):
        return "d_{}_i_{}".format(id_d, id_i)

    @staticmethod
    def _make_argmap_name(id_mss, id_d, id_i, stage_name):
        return "arg_map_{}_{}_{}_{}".format(
            id_mss, id_d, id_i, to_identifier(stage_name)
        )

    @staticmethod
    def _make_intervals(mss_id, mss, stage_info):
        intervals = [
            message_to_interval(interval.interval)
            for d in mss.dependent_stages
            for stage in d.independent_stages
            for interval in stage_info[stage.name].intervals
        ]
        levels = list(
            sorted(
                dict.fromkeys(
                    [i.begin for i in intervals] + [i.end + (0, 1) for i in intervals]
                )
            )
        )

        return [
            {
                "interval": (begin, end + (0, -1)),
                "stages": [
                    [
                        {
                            "name": Generator._restore_stage_name(stage_ref.name),
                            "id": Generator._make_stage_id(id_d, id_i),
                            "argmap_name": Generator._make_argmap_name(
                                mss_id,
                                id_d,
                                id_i,
                                Generator._restore_stage_name(stage_ref.name),
                            ),
                            "overload": (
                                message_to_interval(i.interval)
                                if i.overload == interface_pb2.StageInterval.INTERVAL
                                else "none"
                            ),
                        }
                        for id_i, stage_ref in enumerate(d.independent_stages)
                        for i in stage_info[stage_ref.name].intervals
                        if message_to_interval(i.interval).contains(begin)
                    ]
                    for id_d, d in enumerate(mss.dependent_stages)
                ],
            }
            for begin, end in zip(levels[:-1], levels[1:])
        ]

    @staticmethod
    def _arg_type(arg):
        if arg.arg_type == interface_pb2.Multistage.NORMAL:
            return "normal"
        else:
            return "temporary"

    def _all_kinds(self, mss):
        return list(
            dict.fromkeys(
                [
                    self.computation.fields.args[arg_ref.id].kind
                    for d in mss.dependent_stages
                    for stage_ref in d.independent_stages
                    for arg_ref in stage_ref.args
                    if arg_ref.arg_type != interface_pb2.Multistage.TEMPORARY
                ]
            )
        )

    def _all_args(self, mss):
        Arg = collections.namedtuple("Arg", ["id", "temporary", "readonly"])
        all_args = [
            Arg(
                arg_ref.id,
                arg_ref.arg_type == interface_pb2.Multistage.TEMPORARY,
                accessor.intent == interface_pb2.Accessor.READ_ONLY,
            )
            for d in mss.dependent_stages
            for stage_ref in d.independent_stages
            for arg_ref, accessor in zip(
                stage_ref.args, self.computation.stages[stage_ref.name].accessors
            )
        ]
        # now reduce readonly-ness
        return list(
            map(
                lambda x: reduce(
                    lambda arg1, arg2: Arg(
                        arg1.id, arg1.temporary, arg1.readonly and arg2.readonly
                    ),
                    x[1],
                ),
                groupby(sorted(all_args), lambda arg: (arg.id, arg.temporary)),
            )
        )

    def _patch_mss_data_with_caches(self, mss_data, mss):
        for k_cache in mss.k_caches:
            kind = (
                self.computation.fields.args[k_cache.id].kind
                if not k_cache.temporary
                else None
            )
            local = not k_cache.fill and not k_cache.flush
            if not k_cache.temporary:
                mss_data["kinds"][kind]["args"][k_cache.id]["cached"] = "K"
                mss_data["kinds"][kind]["args"][k_cache.id]["local"] = local
            else:
                mss_data["temporaries"]["args"][k_cache.id]["cached"] = "K"
                mss_data["temporaries"]["args"][k_cache.id]["local"] = local

        for ij_cache in mss.ij_caches:
            if not ij_cache.temporary:
                kind = self.computation.fields.args[ij_cache.id].kind
                mss_data["kinds"][kind]["args"][ij_cache.id]["cached"] = "IJ"
                mss_data["kinds"][kind]["args"][ij_cache.id]["local"] = True
            else:
                mss_data["temporaries"]["args"][ij_cache.id]["cached"] = "IJ"
                mss_data["temporaries"]["args"][ij_cache.id]["local"] = True

    def _patch_context_with_caches(self, context):
        for mss in self.computation.multistages:
            for k_cache in mss.k_caches:
                local = not k_cache.fill and not k_cache.flush
                if not k_cache.temporary:
                    context["args"][k_cache.id]["local"] = local
                else:
                    context["temporaries"][k_cache.id]["local"] = local

            for ij_cache in mss.ij_caches:
                if not ij_cache.temporary:
                    context["args"][ij_cache.id]["local"] = True
                else:
                    context["temporaries"][ij_cache.id]["local"] = True

    def _is_readonly(self, mss_data, arg_id, temporary):
        if temporary:
            return mss_data["temporaries"]["args"][arg_id]["readonly"]
        else:
            kind = self.computation.fields.args[arg_id].kind
            return mss_data["kinds"][kind]["args"][arg_id]["readonly"]

    def _make_multistage(self, mss_id, mss, mss_stage_analysis):
        mss_data = {
            "id": mss_id,
            "intervals": self._make_intervals(mss_id, mss, self.computation.stages),
            "max_stage_extent": reduce(
                lambda l, r: max_extent(l, reduce(max_extent, r)),
                mss_stage_analysis,
                (0, 0, 0, 0),
            ),
            "direction": (
                "forward"
                if mss.policy == interface_pb2.Multistage.FORWARD
                else (
                    "backward"
                    if mss.policy == interface_pb2.Multistage.BACKWARD
                    else "parallel"
                )
            ),
            "blocksize": 20 if mss.policy == interface_pb2.Multistage.PARALLEL else None,
            "stages": [
                [
                    {
                        "stage_extent": stage_extent,
                        "name": stage_ref.name.replace("(anonymous namespace)::", ""),
                        "id": Generator._make_stage_id(id_d, id_i),
                        "argmap_name": Generator._make_argmap_name(
                            mss_id, id_d, id_i, Generator._restore_stage_name(stage_ref.name)
                        ),
                        "argmap": [
                            {
                                "accessor": accessor.id,
                                "arg": {"type": Generator._arg_type(arg), "id": arg.id},
                            }
                            for arg, accessor in zip(
                                stage_ref.args,
                                self.computation.stages[stage_ref.name].accessors,
                            )
                        ],
                    }
                    for id_i, (stage_ref, stage_extent) in enumerate(
                        zip(d.independent_stages, d_stage_extents)
                    )
                ]
                for id_d, (d, d_stage_extents) in enumerate(
                    zip(mss.dependent_stages, mss_stage_analysis)
                )
            ],
            "temporaries": {
                "args": {
                    arg_id: {
                        "readonly": readonly,
                        "type": self.computation.temporaries[arg_id].type,
                        "cached": None,
                    }
                    for (arg_id, temporary, readonly) in self._all_args(mss)
                    if temporary
                }
            },
            "kinds": {
                kind: {
                    "args": {
                        arg_id: {
                            "readonly": readonly,
                            "type": self.computation.fields.args[arg_id].type,
                            "cached": None,
                        }
                        for (arg_id, temporary, readonly) in self._all_args(mss)
                        if not temporary
                        and self.computation.fields.args[arg_id].kind == kind
                    }
                }
                for kind in self._all_kinds(mss)
            },
            "ij_caches": [
                {
                    "id": ij_cache.id,
                    "temporary": ij_cache.temporary,
                    "type": ij_cache.type,
                }
                for ij_cache in mss.ij_caches
            ],
            "launch_kernel": "launch_kernel_{}".format(mss_id),
        }

        k_extents = {}
        for d, d_stage_extents in zip(mss.dependent_stages, mss_stage_analysis):
            for stage_ref, stage_extent in zip(d.independent_stages, d_stage_extents):
                stage = self.computation.stages[stage_ref.name]
                for arg, accessor in zip(stage_ref.args, stage.accessors):
                    if not (arg.arg_type, arg.id) in k_extents:
                        k_extents[(arg.arg_type, arg.id)] = (
                            accessor.extent.kminus,
                            accessor.extent.kplus,
                        )
                    else:
                        old = k_extents[(arg.arg_type, arg.id)]
                        k_extents[(arg.arg_type, arg.id)] = (
                            min(old[0], accessor.extent.kminus),
                            max(old[1], accessor.extent.kplus),
                        )
        mss_data["k_caches"] = [
            {
                "id": k_cache.id,
                "temporary": k_cache.temporary,
                "fill": k_cache.fill,
                "flush": k_cache.flush,
                "type": k_cache.type,
                "min_extent": k_extents[(k_cache.temporary, k_cache.id)][0],
                "max_extent": k_extents[(k_cache.temporary, k_cache.id)][1],
                "kind": (
                    None
                    if k_cache.temporary
                    else self.computation.fields.args[k_cache.id].kind
                ),
                "readonly": (
                    None
                    if not k_cache.fill and not k_cache.flush
                    else self._is_readonly(mss_data, k_cache.id, k_cache.temporary)
                ),
            }
            for k_cache in mss.k_caches
        ]

        self._patch_mss_data_with_caches(mss_data, mss)

        return mss_data

    def _patch_computation(self, context):
        readwrite_args = [arg_id
            for mss in context["multistages"]
            for kind_id, kind_info in mss["kinds"].items()
            for arg_id, arg_info in kind_info["args"].items()
            if not arg_info["readonly"]]

        for arg_id, arg_info in context["args"].items():
            arg_info["readonly"] = not arg_id in readwrite_args

    def generate(self, out_file):
        stage_analysis_data, arg_extents = self._stage_analysis()

        context = {
            "hash": self.computation_id,
            "name": self.computation_name,
            "max_arg_extent": reduce(max_extent, arg_extents.values()),
            "offset_limit": self.computation.offset_limit,
            "kinds": {
                kind_id: {
                    "layout": [-1, -1, -1] if list(kind_info.layout) == [-1] else list(kind_info.layout),
                    "args": [
                        arg_id
                        for arg_id, arg_info in self.computation.fields.args.items()
                        if arg_info.kind == kind_id
                    ],
                }
                for kind_id, kind_info in self.computation.fields.kinds.items()
            },
            "args": {
                arg_id: {
                    "kind": arg_info.kind,
                    "type": arg_info.type,
                    "extent": arg_extents[arg_id, interface_pb2.Multistage.NORMAL],
                }
                for arg_id, arg_info in self.computation.fields.args.items()
            },
            "temporaries": {
                field_id: {
                    "selector": list(field_info.selector),
                    "type": field_info.type,
                }
                for field_id, field_info in self.computation.temporaries.items()
            },
            "multistages": [
                self._make_multistage(mss_id, mss, mss_stage_analysis)
                for mss_id, (mss, mss_stage_analysis) in enumerate(
                    zip(self.computation.multistages, stage_analysis_data)
                )
            ],
        }
        self._patch_context_with_caches(context)
        self._patch_computation(context)

        pprint(context)

        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(
                os.path.join(os.path.dirname(__file__), "./templates")
            ),
            autoescape=False,
            extensions=["jinja2.ext.do"],
        )

        class JoinerFirst(object):
            def __init__(self, sep=", ", first=""):
                self.first = first
                self.sep = sep
                self.used = False

            def __call__(self):
                if not self.used:
                    self.used = True
                    return self.first
                return self.sep

        env.globals["joiner_first"] = JoinerFirst

        env.filters.update(
            {
                "kind_dimension": lambda k: sum(
                    1
                    for v in (k["layout"] if "layout" in k else k["selector"])
                    if v != -1
                ),
                "masked": lambda k, i: (
                    k["layout"][i] == -1 if "layout" in k else k["selector"][i]
                ),
                "unitstride": lambda k, i: (
                    k["layout"][i] == 0
                    if "layout" in k
                    else not any(k["selector"][0:i])
                ),
                "has_kcaches": lambda multistage: (
                    "k_caches" in multistage.keys() and len(multistage["k_caches"]) > 0
                ),
                "to_identifier": to_identifier,
                "bool_to_str": lambda b: "true" if b else "false",
                # TODO: Figure out why I need to multiply with 1...
                "level_diff": lambda i: 1 * i[1][1] - 1 * i[0][1] - (1 if 1 * i[0][1] < 0 and 1 * i[1][1] > 0 else 0) + 1,
            }
        )

        template = env.get_template("generator.cpp.j2")

        with open(out_file, "w") as out_f:
            print(template.render(context=context), end="", file=out_f)

        os.system("clang-format -i {}".format(out_file))

in_file = os.path.abspath(sys.argv[1])
out_file = os.path.abspath(sys.argv[2])

if in_file.endswith("_expanded"):
    assert out_file.endswith("_expanded")
    expanded_file = out_file[:-len("_expanded")]
    print("<{}_expanded>".format(expanded_file))

    with open(expanded_file, "w") as out_f:
        comp_name = in_file.split("__")[-1][:-len("_expanded")]
        in_file_common = in_file[:-len("_expanded")]
        comp_id = int(
            hashlib.blake2b(in_file_common.encode("utf-8"), digest_size=7).hexdigest(), 16
        )
        print(comp_name + "_remainder")
        comp_remainder_id = int(
            hashlib.blake2b((in_file_common + "_remainder").encode("utf-8"), digest_size=7).hexdigest(), 16
        )
        comp_expandable_id = int(
            hashlib.blake2b((in_file_common + "_expanded").encode("utf-8"), digest_size=7).hexdigest(), 16
        )

        print("#define GT_DUMP_IDENTIFIER_{} {}".format(comp_name, comp_id), file=out_f)
        print("namespace gridtools {", file=out_f)
        print("""
            template <>
            struct expandable_computation_mapper<{}> {{
                static constexpr long int expandable = {};
                static constexpr long int remainder = {};
            }};""".format(comp_id, comp_expandable_id, comp_remainder_id), file=out_f)
        print("}", file=out_f)
        print("#include <{}_expanded>".format(expanded_file), file=out_f)
        print("#include <{}_remainder>".format(expanded_file), file=out_f)

    os.system("clang-format -i {}".format(expanded_file))

Generator(in_file).generate(out_file)

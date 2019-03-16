import sys
import os
import re

import hashlib

in_file = sys.argv[1]
out_file = sys.argv[2]

with open(sys.argv[2], 'w') as out_f:
    h = hashlib.blake2b(sys.argv[1].encode('utf-8'), digest_size=7)
    stencil_name = sys.argv[1].split("__")[-1]
    out = """
        #define GT_DUMP_IDENTIFIER_{stencil_name} {stencil_hash}
        // this code was generated
        namespace gridtools {{
            template <>
            struct generated_computation<std::integral_constant<long int, {stencil_hash}>> {{
                void run() {{ std::cout << "run" << std::endl; }}
                void reset_meter() {{}}
                std::string print_meter() const {{ return ""; }}
            }};
        }} // namespace gridtools
        """.format(stencil_hash=str(int(h.hexdigest(), 16)), stencil_name=stencil_name)
    print(out, end='', file=out_f)

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

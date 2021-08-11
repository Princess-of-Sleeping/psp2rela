# psp2rela

The module relocation config optimization tool.

Optimize the relocation config contained in modules created with the Vita SDK or any SDK derived from them, or any other SDK.

# Build

## Requires

- Ubuntu or WSL2 or a machine that can build this project

- Install zlib library

When you have everything you need, run `./build.sh` and wait for the build to complete.

# How to use

Specify the `-src` and `-dst` arguments on the command line.

Example : `./psp2rela -src=./input_module.self -dst=./output_module.self`

# Information

A. VitaSDK uses all type0.

B. Official external SDK uses only type0 and type1.

C. Official internal SDK uses all.

type0 is 12-bytes, type1 is 8-bytes.

This tool should work well with the A and B SDKs and reduce the module size.

The ones created with the C SDK are already fully optimized as they are already using all types. Therefore, using this tool does not give effective expectations.

### TODO

- [ ] Check the validity of the module more.
- [ ] Optimize code more

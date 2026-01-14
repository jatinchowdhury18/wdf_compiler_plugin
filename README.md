# wdf_compiler_plugin

This repository contains an audio plugin, meant to serve as an example and prototyping tool for the [`wdf_compiler`](https://github.com/Chowdhury-DSP/wdf_compiler) project.

## Building

To build from scratch, you must have CMake installed.

```bash
# Clone the repository
$ git clone https://github.com/jatinchowdhury18/wdf_compiler_plugin.git
$ cd wdf_compiler_plugin

# build with CMake
$ cmake -Bbuild
$ cmake --build build --config Release
```

## Usage

To use this plugin, you must first have the following installed:
- A JAI compiler (currently tested with beta version 0.2.023)
- `wdf_compiler`

The first time you run the plugin, the plugin will create a config file,
which can be accessed from the "Settings" menu. From here you can set the
paths for the two compilers listed above, as well as the circuit that you
want to have continuously reloaded.

## License

ChowProtoPlug is open source, and is licensed under the MIT license.
Enjoy!

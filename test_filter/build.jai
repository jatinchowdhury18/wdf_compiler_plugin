#import "Compiler";
#import "Basic";

#run {
    w := compiler_create_workspace();
    target_options := get_build_options(w);
    target_options.output_type = .DYNAMIC_LIBRARY; // specifies output to be a dynamic library
    target_options.output_executable_name = CIRCUIT_NAME;

    import_path: [..] string;
    array_add(*import_path, ..target_options.import_path);
    array_add(*import_path, COMPILER_DIR);
    target_options.import_path = import_path;
    set_build_options(target_options, w);

    compiler_begin_intercept(w);
    add_build_file("main.jai", w);
    compiler_end_intercept(w);

    set_build_options_dc(.{do_output=false});  // No executable for this workspace.
}

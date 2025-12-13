#import "Basic";
#import "Compiler";
File :: #import "File";
String :: #import "String";
Reflection :: #import "Reflection";
Math :: #import "Math";

// @TODO: don't hardcode these paths
#if OS == {
    case .MACOS;
        Parser :: #import,file "/Users/jatin/ChowDSP/Research/WaveDigitalFilters/wdf_compiler/compiler/wdf_parser.jai";
    case .WINDOWS;
        Parser :: #import,file "/Users/jatin/ChowDSP/Research/WaveDigitalFilters/wdf_compiler/compiler/wdf_parser.jai";
}

#load "test_filter.jai"; // @TODO: we could have this automated?

Param_Info :: struct {
    name: string;
    default: float;
    start: float;
    end: float;
    center: float;
    reference: *float;
}

parse_param_info :: (wdf_file: string, proc: *Processor) -> [..] Param_Info {
    info: [..] Param_Info;

    file_str, fread_success := File.read_entire_file(wdf_file);
    assert(fread_success);

    sections := Parser.parse_file_sections (file_str);
    circuit_tokens := Parser.parse_circuit(sections.circuit);
    info_lines := String.split(sections.meta, cast(u8) #char "\n");
    for line : info_lines {
        line_trim := String.trim(line);
        was_split, key, value := String.split_from_left(line_trim, cast(u8) #char ":");
        if was_split == false { continue; }

        key = String.trim(key);
        value = String.trim(value);
        if (key == "param") {
            param_meta := String.split(value, cast(u8) #char ",");
            if (param_meta.count < 3) { continue; }
            reference: *float = null;
            for token : circuit_tokens {
                for param_name : token.param_names {
                    if param_name == param_meta[0] {
                        field_info := Reflection.get_struct_field_info(type_info(Params), param_name);
                        assert(field_info != null);
                        reference = cast(*float)(cast(*u8)(*proc.params) + field_info.offset_in_bytes);
                        break token;
                    }
                }
            }
            if reference == null { continue; }

            param_info: Param_Info;
            param_info.name = copy_string(param_meta[0]);
            param_info.reference = reference;
            param_info.start = Parser.get_value(param_meta[1]);
            param_info.end = Parser.get_value(param_meta[2]);
            param_info.center = ifx param_meta.count >= 4
                then Parser.get_value(param_meta[3])
                else Math.sqrt(param_info.start * param_info.end);
            param_info.default = ifx param_meta.count >= 5
                then Parser.get_value(param_meta[4])
                else param_info.center;

            array_add(*info, param_info);
        }
    }

    return info;
}

Processor :: struct {
    impedances: Impedances;
    state: State;
    fs: float;

    params: Params;
    param_info: [..] Param_Info;
}

#program_export
prepare :: (processor: *void, sample_rate: float64, samples_per_block: s32) #c_call {
    push_context {
        print("Preparing processor with sample rate: %, and buffer size: %", sample_rate, samples_per_block);
        (cast(*Processor) processor).fs = cast(float) sample_rate;
    }
    reset(processor);
}

#program_export
reset :: (processor: *void) #c_call {
    push_context {
        (cast(*Processor) processor).state = .{};
    }
}

#program_export
process :: (processor: *void, data: *float, N: int) #c_call {
    push_context {
        proc := cast(*Processor) processor;
        calc_impedances(*proc.impedances, proc.fs, proc.params);

        for n : 0..(N-1) {
            data[n] = process(*proc.state, *proc.impedances, data[n]);
        }
    }
}

#program_export
get_num_float_params :: (processor: *void) -> int #c_call {
    push_context {
        proc := cast(*Processor) processor;
        return proc.param_info.count;
    }
}

#program_export
get_float_param_info :: (processor: *void,
                         param_index: int,
                         name: *u8, // max size: 128
                         default_value: *float,
                         start: *float,
                         end: *float,
                         center: *float) #c_call {
    push_context {
        proc := cast(*Processor) processor;
        if param_index >= proc.param_info.count { return; }
        param_info := proc.param_info[param_index];
        memcpy(name, param_info.name.data, param_info.name.count);
        default_value.* = param_info.default;
        start.* = param_info.start;
        end.* = param_info.end;
        center.* = param_info.center;
    }
}

#program_export
set_float_param :: (processor: *void, param_index: int, value: float) #c_call {
    push_context {
        proc := cast(*Processor) processor;
        if param_index >= proc.param_info.count { return; }
        proc.param_info[param_index].reference.* = value;
    }
}

#program_export
create_processor :: (wdf_file_path: *u8) -> *void #c_call {
    push_context {
        proc := New(Processor);
        proc.param_info = parse_param_info (to_string (wdf_file_path), proc);
        return proc;
    }
}

#program_export
delete_processor :: (processor: *void) #c_call {
    push_context {
        proc := cast(*Processor) processor;
        array_free(proc.param_info);
        free(proc);
    }
}

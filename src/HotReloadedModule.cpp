#include "HotReloadedModule.h"
#include <chowdsp_logging/chowdsp_logging.h>

namespace
{
using namespace chowdsp::string_literals;
constexpr auto create_proc_tag = "create_processor"_sl;
constexpr auto delete_proc_tag = "delete_processor"_sl;
constexpr auto prepare_proc_tag = "prepare"_sl;
constexpr auto reset_proc_tag = "reset"_sl;
constexpr auto process_proc_tag = "process"_sl;
constexpr auto get_num_float_params_tag = "get_num_float_params"_sl;
constexpr auto get_float_param_info_tag = "get_float_param_info"_sl;
constexpr auto set_float_param_tag = "set_float_param"_sl;

auto get_wdf_source_path (const ModuleConfig& config)
{
    const auto module_dir = juce::File { config.module_dir };
    return module_dir.getChildFile (module_dir.getFileName()).withFileExtension ("wdf");
}

auto get_wdf_output_path (const ModuleConfig& config)
{
    const auto module_dir = juce::File { config.module_dir };
    return module_dir.getChildFile (module_dir.getFileName()).withFileExtension ("jai");
}

auto get_jai_build_path (const ModuleConfig& config)
{
    const auto module_dir = juce::File { config.module_dir };
    return module_dir.getChildFile ("build.jai");
}

auto get_dll_bin_path (const ModuleConfig& config)
{
    const auto module_dir = juce::File { config.module_dir };
    return module_dir.getChildFile (module_dir.getFileName()).withFileExtension ("dylib");
}

auto get_compile_command (const ModuleConfig& config)
{
    const auto wdf_compile_command = juce::String { config.wdf_compiler_path } + " "
                                     + get_wdf_source_path (config).getFullPathName()
                                     + " " + get_wdf_output_path (config).getFullPathName()
                                     + " -lang jai";
    const auto dll_compile_command = juce::String { config.jai_compiler_path }
                                     + " " + get_jai_build_path (config).getFullPathName();
    return wdf_compile_command + " && " + dll_compile_command;
}

struct Command
{
    int result = 0;
    juce::String output {};
    std::chrono::duration<double> duration {};

    explicit Command (const juce::String& cmd)
    {
        // @TODO
        const auto out_file = juce::File { "/Users/jatin/Desktop/wdf_compiler_out.txt" }; // ModuleConfig::config_file.getSiblingFile ("cmd_out.txt");
        [[maybe_unused]] const auto _ = out_file.create();
        const auto full_cmd = cmd + " >" + out_file.getFullPathName();

        const auto start = std::chrono::steady_clock::now();
        result = std::system (full_cmd.toRawUTF8());
        const auto end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::duration<double>> (end - start);

        output = out_file.loadFileAsString()
#if JUCE_WINDOWS
                     .upToLastOccurrenceOf ("\r\n", false, false);
#else
                     .upToLastOccurrenceOf ("\n", false, false);
#endif
    }
};
} // namespace

HotReloadedModule::HotReloadedModule()
{
    old_cout_buffer = std::cout.rdbuf (&logging_buffer);
}

HotReloadedModule::~HotReloadedModule()
{
    close_dll();
    std::cout.rdbuf (old_cout_buffer);
}

void HotReloadedModule::update_config (const ModuleConfig& new_config)
{
    config = new_config;

    dll_source_file_changed();

    file_watcher.emplace (get_wdf_source_path (config));
    file_watcher->on_file_change = [this]
    { dll_source_file_changed(); };
}

void HotReloadedModule::dll_source_file_changed()
{
    juce::Logger::writeToLog ("-----------------------------------------");
    juce::Logger::writeToLog ("Re-compiling module!");

    {
        juce::GenericScopedLock dll_lock { dll_reloading_mutex };
        close_dll();
    }

    Command build_command { get_compile_command (config) };
    chowdsp::log ("Compilation completed in {:.2f} seconds", build_command.duration.count());

    const auto exit_code = build_command.result;
    if (exit_code == 0)
    {
        load_dll();
    }
    else
    {
        chowdsp::log ("Compiler failed with exit code: {}", exit_code);
        chowdsp::log ("Compiler logs: {}", build_command.output);
    }
}

void HotReloadedModule::load_dll()
{
    juce::GenericScopedLock dll_lock { dll_reloading_mutex };

    const auto module_path = get_dll_bin_path (config).getFullPathName();
    chowdsp::log ("Loading module from path: {}", module_path);
    dll.open (module_path);

    const auto func_table_loaded = load_function_table();
    if (! func_table_loaded)
    {
        juce::Logger::writeToLog ("Failed to load functions from DLL!");
        dll.close();
        return;
    }

    const auto wdf_source_path = get_wdf_source_path (config).getFullPathName();
    processor_data[0] = create_proc_func (const_cast<char*> (wdf_source_path.toRawUTF8()));
    processor_data[1] = create_proc_func (const_cast<char*> (wdf_source_path.toRawUTF8()));
    load_parameters();
    if (process_spec.sampleRate > 0.0)
    {
        for (auto* data : processor_data)
            prepare_proc_func (data, process_spec.sampleRate, static_cast<int> (process_spec.maximumBlockSize));
    }
}

bool HotReloadedModule::load_function_table()
{
    create_proc_func = reinterpret_cast<Create_Proc_Func> (dll.getFunction (create_proc_tag));
    destroy_proc_func = reinterpret_cast<Destroy_Proc_Func> (dll.getFunction (delete_proc_tag));
    prepare_proc_func = reinterpret_cast<Prepare_Proc_Func> (dll.getFunction (prepare_proc_tag));
    reset_proc_func = reinterpret_cast<Reset_Proc_Func> (dll.getFunction (reset_proc_tag));
    process_proc_func = reinterpret_cast<Process_Proc_Func> (dll.getFunction (process_proc_tag));
    get_num_float_params_func = reinterpret_cast<Get_Num_Float_Params_Func> (dll.getFunction (get_num_float_params_tag));
    get_float_param_info_func = reinterpret_cast<Get_Float_Param_Info_Func> (dll.getFunction (get_float_param_info_tag));
    set_float_param_func = reinterpret_cast<Set_Float_Param> (dll.getFunction (set_float_param_tag));

    // These functions must be provided! All others are allowed to be nullptr.
    if (create_proc_func == nullptr || destroy_proc_func == nullptr || prepare_proc_func == nullptr || reset_proc_func == nullptr
        || process_proc_func == nullptr)
    {
        return false;
    }

    return true;
}

void HotReloadedModule::close_dll()
{
    params->clear_all_params();
    if (processor_data[0] != nullptr)
    {
        for (auto* data : processor_data)
            destroy_proc_func (data);
    }
    clear_function_table();
    std::fill (processor_data.begin(), processor_data.end(), nullptr);
    dll.close();
}

void HotReloadedModule::clear_function_table()
{
    create_proc_func = nullptr;
    destroy_proc_func = nullptr;
    prepare_proc_func = nullptr;
    reset_proc_func = nullptr;
    process_proc_func = nullptr;
    get_num_float_params_func = nullptr;
    get_float_param_info_func = nullptr;
    set_float_param_func = nullptr;
}

void HotReloadedModule::load_parameters()
{
    using namespace chowdsp::ParamUtils;
    if (get_num_float_params_func != nullptr && get_float_param_info_func != nullptr)
    {
        const auto num_params = get_num_float_params_func (processor_data[0]);
        chowdsp::log ("Module contains {:d} float parameters!", num_params);
        for (int i = 0; i < num_params; ++i)
        {
            char name[128] {};
            float default_value, start, end, center;
            get_float_param_info_func (processor_data[0], i, name, &default_value, &start, &end, &center);

            if (name[0] == '\0')
            {
                chowdsp::log ("No param info provided for parameter index: {:d}", i);
                continue;
            }

            std::array range_info { start, center, end };
            chowdsp::log ("Adding parameter: {}, {}, default: {}",
                          name,
                          std::span<float> { range_info },
                          default_value);
            params->float_params.emplace_back (chowdsp::toString (chowdsp::format ("float_param_{}", i)),
                                               juce::String { name },
                                               createNormalisableRange (start, end, center),
                                               default_value);
        }
    }

    params->finished_loading_params();
}

void HotReloadedModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    process_spec = spec;

    if (processor_data[0] != nullptr)
    {
        for (auto* data : processor_data)
            prepare_proc_func (data, process_spec.sampleRate, static_cast<int> (process_spec.maximumBlockSize));
    }
}

void HotReloadedModule::process (const chowdsp::BufferView<float>& buffer) noexcept
{
    juce::GenericScopedTryLock dll_try_lock { dll_reloading_mutex };
    if (! dll_try_lock.isLocked() || processor_data[0] == nullptr)
    {
        buffer.clear();
        return;
    }

    for (auto [ch, buffer_data] : chowdsp::buffer_iters::channels (buffer))
    {
        if (set_float_param_func != nullptr)
        {
            for (const auto [idx, param] : chowdsp::enumerate (params->float_params))
                set_float_param_func (processor_data[(size_t) ch], static_cast<int> (idx), param->getCurrentValue());
        }

        process_proc_func (processor_data[(size_t) ch], buffer_data.data(), (int) buffer_data.size());
    }

    if (! chowdsp::BufferMath::sanitizeBuffer (buffer, 10.0f))
    {
        for (auto* data : processor_data)
            reset_proc_func (data);
    }
}

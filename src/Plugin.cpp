#include "Plugin.h"
#include "PluginEditor.h"

Plugin::Plugin()
{
    params.forwarding_params.emplace (*this, state);
    module.params = &params;

    update_config();
}

void Plugin::update_config()
{
    try
    {
        const auto config_json = chowdsp::JSONUtils::fromFile (config_file);
        juce::Logger::writeToLog (std::string { "Loading config: " } + config_json.dump());
        module.update_config ({
            .jai_compiler_path = config_json.value ("jai_compiler_path", std::string {}),
            .wdf_compiler_path = config_json.value ("wdf_compiler_path", std::string {}),
            .module_dir = config_json.value ("module_directory", std::string {}),
        });
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog (std::string { "Error loading config: " } + e.what());
    }
}

void Plugin::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);

    module.prepare ({
        sampleRate,
        static_cast<uint32_t> (samplesPerBlock),
        static_cast<uint32_t> (getMainBusNumInputChannels()),
    });

    scope_task.prepare (sampleRate, samplesPerBlock, getMainBusNumInputChannels());
    input_spectrum.prepare (sampleRate, samplesPerBlock, getMainBusNumInputChannels());
    output_spectrum.prepare (sampleRate, samplesPerBlock, getMainBusNumInputChannels());
}

void Plugin::processAudioBlock (juce::AudioBuffer<float>& buffer)
{
    input_spectrum.pushSamples (buffer);

    module.process (buffer);

    scope_task.pushSamples (buffer);
    output_spectrum.pushSamples (buffer);
}

juce::AudioProcessorEditor* Plugin::createEditor()
{
    return new PluginEditor { *this };
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Plugin();
}

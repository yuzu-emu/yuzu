// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Include this early to include Vulkan headers how we want to
#include "video_core/vulkan_common/vulkan_wrapper.h"

#include <QColorDialog>
#include <QComboBox>
#include <QVulkanInstance>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_graphics.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_graphics.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphics) {
    vulkan_device = Settings::values.vulkan_device.GetValue();
    RetrieveVulkanDevices();

    ui->setupUi(this);

    for (const auto& device : vulkan_devices) {
        ui->device->addItem(device);
    }

    ui->backend->addItem(QStringLiteral("GLSL"));
    ui->backend->addItem(tr("GLASM (NVIDIA Only)"));
    ui->backend->addItem(QStringLiteral("SPIR-V (Experimental, Mesa Only)"));

    SetupPerGameUI();

    SetConfiguration();

    connect(ui->api, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        UpdateAPILayout();
        if (!Settings::IsConfiguringGlobal()) {
            ConfigurationShared::SetHighlight(
                ui->api_widget, ui->api->currentIndex() != ConfigurationShared::USE_GLOBAL_INDEX);
        }
    });
    connect(ui->device, qOverload<int>(&QComboBox::activated), this,
            [this](int device) { UpdateDeviceSelection(device); });
    connect(ui->backend, qOverload<int>(&QComboBox::activated), this,
            [this](int backend) { UpdateShaderBackendSelection(backend); });

    connect(ui->bg_button, &QPushButton::clicked, this, [this] {
        const QColor new_bg_color = QColorDialog::getColor(bg_color);
        if (!new_bg_color.isValid()) {
            return;
        }
        UpdateBackgroundColorButton(new_bg_color);
    });

    ui->bg_label->setVisible(Settings::IsConfiguringGlobal());
    ui->bg_combobox->setVisible(!Settings::IsConfiguringGlobal());
}

void ConfigureGraphics::UpdateDeviceSelection(int device) {
    if (device == -1) {
        return;
    }
    if (GetCurrentGraphicsBackend() == Settings::RendererBackend::Vulkan) {
        vulkan_device = device;
    }
}

void ConfigureGraphics::UpdateShaderBackendSelection(int backend) {
    if (backend == -1) {
        return;
    }
    if (GetCurrentGraphicsBackend() == Settings::RendererBackend::OpenGL) {
        shader_backend = static_cast<Settings::ShaderBackend>(backend);
    }
}

ConfigureGraphics::~ConfigureGraphics() = default;

void ConfigureGraphics::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->api_widget->setEnabled(runtime_lock);
    ui->use_asynchronous_gpu_emulation->setEnabled(runtime_lock);
    ui->use_disk_shader_cache->setEnabled(runtime_lock);
    ui->use_nvdec_emulation->setEnabled(runtime_lock);
    ui->accelerate_astc->setEnabled(runtime_lock);
    ui->use_disk_shader_cache->setChecked(Settings::values.use_disk_shader_cache.GetValue());
    ui->use_asynchronous_gpu_emulation->setChecked(
        Settings::values.use_asynchronous_gpu_emulation.GetValue());
    ui->use_nvdec_emulation->setChecked(Settings::values.use_nvdec_emulation.GetValue());
    ui->accelerate_astc->setChecked(Settings::values.accelerate_astc.GetValue());

    if (Settings::IsConfiguringGlobal()) {
        ui->api->setCurrentIndex(static_cast<int>(Settings::values.renderer_backend.GetValue()));
        ui->fullscreen_mode_combobox->setCurrentIndex(Settings::values.fullscreen_mode.GetValue());
        ui->aspect_ratio_combobox->setCurrentIndex(Settings::values.aspect_ratio.GetValue());
    } else {
        ConfigurationShared::SetPerGameSetting(ui->api, &Settings::values.renderer_backend);
        ConfigurationShared::SetHighlight(ui->api_widget,
                                          !Settings::values.renderer_backend.UsingGlobal());

        ConfigurationShared::SetPerGameSetting(ui->fullscreen_mode_combobox,
                                               &Settings::values.fullscreen_mode);
        ConfigurationShared::SetHighlight(ui->fullscreen_mode_label,
                                          !Settings::values.fullscreen_mode.UsingGlobal());

        ConfigurationShared::SetPerGameSetting(ui->aspect_ratio_combobox,
                                               &Settings::values.aspect_ratio);
        ConfigurationShared::SetHighlight(ui->ar_label,
                                          !Settings::values.aspect_ratio.UsingGlobal());

        ui->bg_combobox->setCurrentIndex(Settings::values.bg_red.UsingGlobal() ? 0 : 1);
        ui->bg_button->setEnabled(!Settings::values.bg_red.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->bg_layout, !Settings::values.bg_red.UsingGlobal());
    }

    UpdateBackgroundColorButton(QColor::fromRgbF(Settings::values.bg_red.GetValue(),
                                                 Settings::values.bg_green.GetValue(),
                                                 Settings::values.bg_blue.GetValue()));
    UpdateAPILayout();
}

void ConfigureGraphics::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.fullscreen_mode,
                                             ui->fullscreen_mode_combobox);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.aspect_ratio,
                                             ui->aspect_ratio_combobox);

    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_disk_shader_cache,
                                             ui->use_disk_shader_cache, use_disk_shader_cache);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_asynchronous_gpu_emulation,
                                             ui->use_asynchronous_gpu_emulation,
                                             use_asynchronous_gpu_emulation);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_nvdec_emulation,
                                             ui->use_nvdec_emulation, use_nvdec_emulation);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.accelerate_astc, ui->accelerate_astc,
                                             accelerate_astc);

    if (Settings::IsConfiguringGlobal()) {
        // Guard if during game and set to game-specific value
        if (Settings::values.renderer_backend.UsingGlobal()) {
            Settings::values.renderer_backend.SetValue(GetCurrentGraphicsBackend());
        }
        if (Settings::values.shader_backend.UsingGlobal()) {
            Settings::values.shader_backend.SetValue(shader_backend);
        }
        if (Settings::values.vulkan_device.UsingGlobal()) {
            Settings::values.vulkan_device.SetValue(vulkan_device);
        }
        if (Settings::values.bg_red.UsingGlobal()) {
            Settings::values.bg_red.SetValue(static_cast<float>(bg_color.redF()));
            Settings::values.bg_green.SetValue(static_cast<float>(bg_color.greenF()));
            Settings::values.bg_blue.SetValue(static_cast<float>(bg_color.blueF()));
        }
    } else {
        if (ui->api->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
            Settings::values.renderer_backend.SetGlobal(true);
            Settings::values.shader_backend.SetGlobal(true);
            Settings::values.vulkan_device.SetGlobal(true);
        } else {
            Settings::values.renderer_backend.SetGlobal(false);
            Settings::values.renderer_backend.SetValue(GetCurrentGraphicsBackend());
            switch (GetCurrentGraphicsBackend()) {
            case Settings::RendererBackend::OpenGL:
                Settings::values.shader_backend.SetGlobal(false);
                Settings::values.vulkan_device.SetGlobal(true);
                Settings::values.shader_backend.SetValue(shader_backend);
                break;
            case Settings::RendererBackend::Vulkan:
                Settings::values.shader_backend.SetGlobal(true);
                Settings::values.vulkan_device.SetGlobal(false);
                Settings::values.vulkan_device.SetValue(vulkan_device);
                break;
            }
        }

        if (ui->bg_combobox->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
            Settings::values.bg_red.SetGlobal(true);
            Settings::values.bg_green.SetGlobal(true);
            Settings::values.bg_blue.SetGlobal(true);
        } else {
            Settings::values.bg_red.SetGlobal(false);
            Settings::values.bg_green.SetGlobal(false);
            Settings::values.bg_blue.SetGlobal(false);
            Settings::values.bg_red.SetValue(static_cast<float>(bg_color.redF()));
            Settings::values.bg_green.SetValue(static_cast<float>(bg_color.greenF()));
            Settings::values.bg_blue.SetValue(static_cast<float>(bg_color.blueF()));
        }
    }
}

void ConfigureGraphics::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGraphics::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGraphics::UpdateBackgroundColorButton(QColor color) {
    bg_color = color;

    QPixmap pixmap(ui->bg_button->size());
    pixmap.fill(bg_color);

    const QIcon color_icon(pixmap);
    ui->bg_button->setIcon(color_icon);
}

void ConfigureGraphics::UpdateAPILayout() {
    if (!Settings::IsConfiguringGlobal() &&
        ui->api->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
        vulkan_device = Settings::values.vulkan_device.GetValue(true);
        shader_backend = Settings::values.shader_backend.GetValue(true);
        ui->device_widget->setEnabled(false);
        ui->backend_widget->setEnabled(false);
    } else {
        vulkan_device = Settings::values.vulkan_device.GetValue();
        shader_backend = Settings::values.shader_backend.GetValue();
        ui->device_widget->setEnabled(true);
        ui->backend_widget->setEnabled(true);
    }

    switch (GetCurrentGraphicsBackend()) {
    case Settings::RendererBackend::OpenGL:
        ui->backend->setCurrentIndex(static_cast<u32>(shader_backend));
        ui->device_widget->setVisible(false);
        ui->backend_widget->setVisible(true);
        break;
    case Settings::RendererBackend::Vulkan:
        ui->device->setCurrentIndex(vulkan_device);
        ui->device_widget->setVisible(true);
        ui->backend_widget->setVisible(false);
        break;
    }
}

void ConfigureGraphics::RetrieveVulkanDevices() try {
    using namespace Vulkan;

    vk::InstanceDispatch dld;
    const Common::DynamicLibrary library = OpenLibrary();
    const vk::Instance instance = CreateInstance(library, dld, VK_API_VERSION_1_0);
    const std::vector<VkPhysicalDevice> physical_devices = instance.EnumeratePhysicalDevices();

    vulkan_devices.clear();
    vulkan_devices.reserve(physical_devices.size());
    for (const VkPhysicalDevice device : physical_devices) {
        const std::string name = vk::PhysicalDevice(device, dld).GetProperties().deviceName;
        vulkan_devices.push_back(QString::fromStdString(name));
    }

} catch (const Vulkan::vk::Exception& exception) {
    LOG_ERROR(Frontend, "Failed to enumerate devices with error: {}", exception.what());
}

Settings::RendererBackend ConfigureGraphics::GetCurrentGraphicsBackend() const {
    if (Settings::IsConfiguringGlobal()) {
        return static_cast<Settings::RendererBackend>(ui->api->currentIndex());
    }

    if (ui->api->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
        Settings::values.renderer_backend.SetGlobal(true);
        return Settings::values.renderer_backend.GetValue();
    }
    Settings::values.renderer_backend.SetGlobal(false);
    return static_cast<Settings::RendererBackend>(ui->api->currentIndex() -
                                                  ConfigurationShared::USE_GLOBAL_OFFSET);
}

void ConfigureGraphics::SetupPerGameUI() {
    if (Settings::IsConfiguringGlobal()) {
        ui->api->setEnabled(Settings::values.renderer_backend.UsingGlobal());
        ui->device->setEnabled(Settings::values.renderer_backend.UsingGlobal());
        ui->fullscreen_mode_combobox->setEnabled(Settings::values.fullscreen_mode.UsingGlobal());
        ui->aspect_ratio_combobox->setEnabled(Settings::values.aspect_ratio.UsingGlobal());
        ui->use_asynchronous_gpu_emulation->setEnabled(
            Settings::values.use_asynchronous_gpu_emulation.UsingGlobal());
        ui->use_nvdec_emulation->setEnabled(Settings::values.use_nvdec_emulation.UsingGlobal());
        ui->accelerate_astc->setEnabled(Settings::values.accelerate_astc.UsingGlobal());
        ui->use_disk_shader_cache->setEnabled(Settings::values.use_disk_shader_cache.UsingGlobal());
        ui->bg_button->setEnabled(Settings::values.bg_red.UsingGlobal());

        return;
    }

    connect(ui->bg_combobox, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        ui->bg_button->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(ui->bg_layout, index == 1);
    });

    ConfigurationShared::SetColoredTristate(
        ui->use_disk_shader_cache, Settings::values.use_disk_shader_cache, use_disk_shader_cache);
    ConfigurationShared::SetColoredTristate(
        ui->use_nvdec_emulation, Settings::values.use_nvdec_emulation, use_nvdec_emulation);
    ConfigurationShared::SetColoredTristate(ui->accelerate_astc, Settings::values.accelerate_astc,
                                            accelerate_astc);
    ConfigurationShared::SetColoredTristate(ui->use_asynchronous_gpu_emulation,
                                            Settings::values.use_asynchronous_gpu_emulation,
                                            use_asynchronous_gpu_emulation);

    ConfigurationShared::SetColoredComboBox(ui->aspect_ratio_combobox, ui->ar_label,
                                            Settings::values.aspect_ratio.GetValue(true));
    ConfigurationShared::SetColoredComboBox(ui->fullscreen_mode_combobox, ui->fullscreen_mode_label,
                                            Settings::values.fullscreen_mode.GetValue(true));
    ConfigurationShared::InsertGlobalItem(
        ui->api, static_cast<int>(Settings::values.renderer_backend.GetValue(true)));
}

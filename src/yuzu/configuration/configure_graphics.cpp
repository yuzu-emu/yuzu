// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#include <QComboBox>
#include <QVulkanInstance>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_graphics.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGraphics>()) {
    vulkan_device = Settings::values.vulkan_device.GetValue();
    RetrieveVulkanDevices();

    ui->setupUi(this);

    SetupPerGameUI();

    SetConfiguration();

    connect(ui->api, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        UpdateDeviceComboBox();
        if (!Settings::IsConfiguringGlobal()) {
            ConfigurationShared::SetHighlight(
                ui->api_layout, ui->api->currentIndex() != ConfigurationShared::USE_GLOBAL_INDEX);
        }
    });
    connect(ui->device, qOverload<int>(&QComboBox::activated), this,
            [this](int device) { UpdateDeviceSelection(device); });

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

ConfigureGraphics::~ConfigureGraphics() = default;

void ConfigureGraphics::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->api->setEnabled(runtime_lock);
    ui->use_asynchronous_gpu_emulation->setEnabled(runtime_lock);
    ui->use_disk_shader_cache->setEnabled(runtime_lock);
    ui->use_nvdec_emulation->setEnabled(runtime_lock);
    ui->use_disk_shader_cache->setChecked(Settings::values.use_disk_shader_cache.GetValue());
    ui->use_asynchronous_gpu_emulation->setChecked(
        Settings::values.use_asynchronous_gpu_emulation.GetValue());
    ui->use_nvdec_emulation->setChecked(Settings::values.use_nvdec_emulation.GetValue());

    if (Settings::IsConfiguringGlobal()) {
        ui->api->setCurrentIndex(static_cast<int>(Settings::values.renderer_backend.GetValue()));
        ui->aspect_ratio_combobox->setCurrentIndex(Settings::values.aspect_ratio.GetValue());
    } else {
        ConfigurationShared::SetPerGameSetting(ui->api, &Settings::values.renderer_backend);
        ConfigurationShared::SetHighlight(ui->api_layout,
                                          !Settings::values.renderer_backend.UsingGlobal());
        ConfigurationShared::SetPerGameSetting(ui->aspect_ratio_combobox,
                                               &Settings::values.aspect_ratio);

        ui->bg_combobox->setCurrentIndex(Settings::values.bg_red.UsingGlobal() ? 0 : 1);
        ui->bg_button->setEnabled(!Settings::values.bg_red.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->ar_label,
                                          !Settings::values.aspect_ratio.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->bg_layout, !Settings::values.bg_red.UsingGlobal());
    }

    UpdateBackgroundColorButton(QColor::fromRgbF(Settings::values.bg_red.GetValue(),
                                                 Settings::values.bg_green.GetValue(),
                                                 Settings::values.bg_blue.GetValue()));
    UpdateDeviceComboBox();
}

void ConfigureGraphics::ApplyConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        // Guard if during game and set to game-specific value
        if (Settings::values.renderer_backend.UsingGlobal()) {
            Settings::values.renderer_backend.SetValue(GetCurrentGraphicsBackend());
        }
        if (Settings::values.vulkan_device.UsingGlobal()) {
            Settings::values.vulkan_device.SetValue(vulkan_device);
        }
        if (Settings::values.aspect_ratio.UsingGlobal()) {
            Settings::values.aspect_ratio.SetValue(ui->aspect_ratio_combobox->currentIndex());
        }
        if (Settings::values.use_disk_shader_cache.UsingGlobal()) {
            Settings::values.use_disk_shader_cache.SetValue(ui->use_disk_shader_cache->isChecked());
        }
        if (Settings::values.use_asynchronous_gpu_emulation.UsingGlobal()) {
            Settings::values.use_asynchronous_gpu_emulation.SetValue(
                ui->use_asynchronous_gpu_emulation->isChecked());
        }
        if (Settings::values.use_nvdec_emulation.UsingGlobal()) {
            Settings::values.use_nvdec_emulation.SetValue(ui->use_nvdec_emulation->isChecked());
        }
        if (Settings::values.bg_red.UsingGlobal()) {
            Settings::values.bg_red.SetValue(static_cast<float>(bg_color.redF()));
            Settings::values.bg_green.SetValue(static_cast<float>(bg_color.greenF()));
            Settings::values.bg_blue.SetValue(static_cast<float>(bg_color.blueF()));
        }
    } else {
        if (ui->api->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
            Settings::values.renderer_backend.SetGlobal(true);
            Settings::values.vulkan_device.SetGlobal(true);
        } else {
            Settings::values.renderer_backend.SetGlobal(false);
            Settings::values.renderer_backend.SetValue(GetCurrentGraphicsBackend());
            if (GetCurrentGraphicsBackend() == Settings::RendererBackend::Vulkan) {
                Settings::values.vulkan_device.SetGlobal(false);
                Settings::values.vulkan_device.SetValue(vulkan_device);
            } else {
                Settings::values.vulkan_device.SetGlobal(true);
            }
        }

        ConfigurationShared::ApplyPerGameSetting(&Settings::values.aspect_ratio,
                                                 ui->aspect_ratio_combobox);

        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_disk_shader_cache,
                                                 ui->use_disk_shader_cache, use_disk_shader_cache);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_asynchronous_gpu_emulation,
                                                 ui->use_asynchronous_gpu_emulation,
                                                 use_asynchronous_gpu_emulation);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_nvdec_emulation,
                                                 ui->use_nvdec_emulation, use_nvdec_emulation);

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

void ConfigureGraphics::UpdateDeviceComboBox() {
    ui->device->clear();

    bool enabled = false;

    if (!Settings::IsConfiguringGlobal() &&
        ui->api->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
        vulkan_device = Settings::values.vulkan_device.GetValue();
    }
    switch (GetCurrentGraphicsBackend()) {
    case Settings::RendererBackend::OpenGL:
        ui->device->addItem(tr("OpenGL Graphics Device"));
        enabled = false;
        break;
    case Settings::RendererBackend::Vulkan:
        for (const auto& device : vulkan_devices) {
            ui->device->addItem(device);
        }
        ui->device->setCurrentIndex(vulkan_device);
        enabled = !vulkan_devices.empty();
        break;
    }
    // If in per-game config and use global is selected, don't enable.
    enabled &= !(!Settings::IsConfiguringGlobal() &&
                 ui->api->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX);
    ui->device->setEnabled(enabled && !Core::System::GetInstance().IsPoweredOn());
}

void ConfigureGraphics::RetrieveVulkanDevices() {
    vulkan_devices.clear();
    for (const auto& name : Vulkan::RendererVulkan::EnumerateDevices()) {
        vulkan_devices.push_back(QString::fromStdString(name));
    }
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
        ui->aspect_ratio_combobox->setEnabled(Settings::values.aspect_ratio.UsingGlobal());
        ui->use_asynchronous_gpu_emulation->setEnabled(
            Settings::values.use_asynchronous_gpu_emulation.UsingGlobal());
        ui->use_nvdec_emulation->setEnabled(Settings::values.use_nvdec_emulation.UsingGlobal());
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
    ConfigurationShared::SetColoredTristate(ui->use_asynchronous_gpu_emulation,
                                            Settings::values.use_asynchronous_gpu_emulation,
                                            use_asynchronous_gpu_emulation);

    ConfigurationShared::SetColoredComboBox(ui->aspect_ratio_combobox, ui->ar_label,
                                            Settings::values.aspect_ratio.GetValue(true));
    ConfigurationShared::InsertGlobalItem(
        ui->api, static_cast<int>(Settings::values.renderer_backend.GetValue(true)));
}

// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include <QString>
#include <QWidget>
#include "common/settings.h"

namespace ConfigurationShared {
enum class CheckState;
}

namespace Ui {
class ConfigureGraphics;
}

class ConfigureGraphics : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGraphics(QWidget* parent = nullptr);
    ~ConfigureGraphics() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void SetConfiguration();

    void UpdateBackgroundColorButton(QColor color);
    void UpdateAPILayout();
    void UpdateDeviceSelection(int device);
    void UpdateShaderBackendSelection(int backend);

    void RetrieveVulkanDevices();

    void SetupPerGameUI();

    Settings::RendererBackend GetCurrentGraphicsBackend() const;

    std::unique_ptr<Ui::ConfigureGraphics> ui;
    QColor bg_color;

    ConfigurationShared::CheckState use_nvdec_emulation;
    ConfigurationShared::CheckState accelerate_astc;
    ConfigurationShared::CheckState use_disk_shader_cache;
    ConfigurationShared::CheckState use_asynchronous_gpu_emulation;

    std::vector<QString> vulkan_devices;
    u32 vulkan_device{};
    Settings::ShaderBackend shader_backend{};
};

/*
 *  Copyright (C) 2011-2013 Bumblebee Project
 *  Author: Peter Wu <lekensteyn@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#define BBSLEEP_VERSION "0.1"

#define OPTIMUS_DSM_POWERDOWN_PS3 (3 << 24)
#define OPTIMUS_DSM_FLAGS_CHANGED (1)
#define OPTIMUS_DSM_SET_POWERDOWN (OPTIMUS_DSM_POWERDOWN_PS3 | OPTIMUS_DSM_FLAGS_CHANGED)

#define OPTIMUS_DSM_REVID 0x100
#define OPTIMUS_DSM_FUNC 0x1A
static const guid_t optimus_dsm_muid =
    GUID_INIT(0xA486D8F8, 0x0BDA, 0x471B,
            0xA7, 0x2B, 0x60, 0x42, 0xA6, 0xB5, 0xBE, 0xE0);

static int bbsleep_dsm(acpi_handle handle, const guid_t *guid, int revid,
        int func, int arg, uint32_t *result) {
    int i;
    union acpi_object *obj;
    char args_buff[4];
    union acpi_object argv4 = {
        .buffer.type = ACPI_TYPE_BUFFER,
        .buffer.length = 4,
        .buffer.pointer = args_buff
    };

    /* ACPI is little endian, AABBCCDD becomes { DD, CC, BB, AA } */
    for (i = 0; i < 4; i++) {
        args_buff[i] = (arg >> i * 8) & 0xFF;
    }

    obj = acpi_evaluate_dsm(handle, guid, revid, func, &argv4);
    if (!obj) {
        acpi_handle_info(handle, "failed to evaluate _DSM\n");
        return AE_ERROR;
    }

    if (obj->type == ACPI_TYPE_INTEGER) {
        *result = obj->integer.value;
    } else if (obj->type == ACPI_TYPE_BUFFER && obj->buffer.length == 4) {
        *result = 0;
        *result |= obj->buffer.pointer[0];
        *result |= (obj->buffer.pointer[1] << 8);
        *result |= (obj->buffer.pointer[2] << 16);
        *result |= (obj->buffer.pointer[3] << 24);
    }

    ACPI_FREE(obj);

    return 0;
}

static int bbsleep_optimus_dsm(acpi_handle handle) {
    uint32_t result = 0;

    if (bbsleep_dsm(handle, &optimus_dsm_muid, OPTIMUS_DSM_REVID,
            OPTIMUS_DSM_FUNC, OPTIMUS_DSM_SET_POWERDOWN, &result)) {
        return 1;
    }

    pr_info("%s: optimus _DSM command evaluated successfully, result: %08X\n",
            __func__, result);

    return 0;
}

static int bbsleep_pci_runtime_suspend(struct device *dev) {
    struct pci_dev *pdev = to_pci_dev(dev);
    acpi_handle handle = ACPI_HANDLE(&pdev->dev);

    bbsleep_optimus_dsm(handle);

    pci_save_state(pdev);
    pci_set_power_state(pdev, PCI_D3cold);

    pr_info("%s: suspending dedicated GPU\n", __func__);

    return 0;
}

static int bbsleep_pci_runtime_resume(struct device *dev) {
    struct pci_dev *pdev = to_pci_dev(dev);

    pci_set_power_state(pdev, PCI_D0);
    pci_restore_state(pdev);

    pr_info("%s: resuming dedicated GPU\n", __func__);

    return 0;
}

static const struct dev_pm_ops bbsleep_pci_pm_ops = {
    .runtime_suspend = bbsleep_pci_runtime_suspend,
    .runtime_resume = bbsleep_pci_runtime_resume,
};

static int bbsleep_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
    pm_runtime_set_active(&pdev->dev);
    pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
    pm_runtime_use_autosuspend(&pdev->dev);
    pm_runtime_allow(&pdev->dev);
    pm_runtime_put_autosuspend(&pdev->dev);

    return 0;
}

static void bbsleep_pci_remove(struct pci_dev *pdev) {
    pm_runtime_get_noresume(&pdev->dev);
    pm_runtime_dont_use_autosuspend(&pdev->dev);
    pm_runtime_forbid(&pdev->dev);
}

static const struct pci_device_id pciidlist[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID),
            PCI_CLASS_DISPLAY_3D << 8, 0xffff00 },
    { 0, 0, 0 },
};

static struct pci_driver bbsleep_pci_driver = {
    .name = KBUILD_MODNAME,
    .id_table = pciidlist,
    .probe  = bbsleep_pci_probe,
    .remove = bbsleep_pci_remove,
    .driver.pm = &bbsleep_pci_pm_ops,
};

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Toggle the discrete graphics card");
MODULE_AUTHOR("Peter Wu <lekensteyn@gmail.com>");
MODULE_VERSION(BBSLEEP_VERSION);

module_pci_driver(bbsleep_pci_driver);

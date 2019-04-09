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

#define OPTIMUS_DSM_REVID 0x100
#define OPTIMUS_DSM_FUNC 0x1A
static const char acpi_optimus_dsm_muid[16] = {
    0xF8, 0xD8, 0x86, 0xA4, 0xDA, 0x0B, 0x1B, 0x47,
    0xA7, 0x2B, 0x60, 0x42, 0xA6, 0xB5, 0xBE, 0xE0,
};

static int acpi_call_dsm(acpi_handle handle, const char muid[16], int revid,
        int func, char args[4], uint32_t *result) {
    struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
    struct acpi_object_list input;
    union acpi_object params[4];
    union acpi_object *obj;
    int err;

    input.count = 4;
    input.pointer = params;
    params[0].type = ACPI_TYPE_BUFFER;
    params[0].buffer.length = 16;
    params[0].buffer.pointer = (char *)muid;

    params[1].type = ACPI_TYPE_INTEGER;
    params[1].integer.value = revid;

    params[2].type = ACPI_TYPE_INTEGER;
    params[2].integer.value = func;

    /*
     * Although the ACPI spec defines Arg3 as a Package, implementations
     * expect a Buffer (CreateWordField and Index functions are
     * applied to it).
     */
    params[3].type = ACPI_TYPE_BUFFER;
    params[3].buffer.length = 4;
    params[3].buffer.pointer = args;

    err = acpi_evaluate_object(handle, "_DSM", &input, &output);
    if (err) {
        pr_err("%s: failed to evaluate _DSM command", __func__);
        return err;
    }

    obj = (union acpi_object *)output.pointer;
    if (obj->type == ACPI_TYPE_INTEGER && result) {
        *result = obj->integer.value;
    } else if (obj->type == ACPI_TYPE_BUFFER && obj->buffer.length == 4) {
        *result = 0;
        *result |= obj->buffer.pointer[0];
        *result |= (obj->buffer.pointer[1] << 8);
        *result |= (obj->buffer.pointer[2] << 16);
        *result |= (obj->buffer.pointer[3] << 24);
    } else {
        pr_err("%s: unsupported result type for _DSM command: %#x\n", __func__,
                obj->type);
    }

    kfree(output.pointer);

    return 0;
}

static int bbsleep_optimus_dsm(acpi_handle handle) {
    char args[] = { 1, 0, 0, 3 };
    uint32_t result = 0;

    if (acpi_call_dsm(handle, acpi_optimus_dsm_muid, OPTIMUS_DSM_REVID,
            OPTIMUS_DSM_FUNC, args, &result)) {
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

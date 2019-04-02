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
#include <asm/uaccess.h>
#include <linux/suspend.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>

#define BBSWITCH_VERSION "0.8"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Toggle the discrete graphics card");
MODULE_AUTHOR("Peter Wu <lekensteyn@gmail.com>");
MODULE_VERSION(BBSWITCH_VERSION);

static const char acpi_optimus_dsm_muid[16] = {
    0xF8, 0xD8, 0x86, 0xA4, 0xDA, 0x0B, 0x1B, 0x47,
    0xA7, 0x2B, 0x60, 0x42, 0xA6, 0xB5, 0xBE, 0xE0,
};

static int acpi_call_dsm(acpi_handle handle, const char muid[16], int revid,
        int func, char args[4]) {
    struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
    struct acpi_object_list input;
    union acpi_object params[4];
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
     * Although the ACPI spec defines Arg3 as a Package, in practise
     * implementations expect a Buffer (CreateWordField and Index functions are
     * applied to it).
     */
    params[3].type = ACPI_TYPE_BUFFER;
    params[3].buffer.length = 4;
    params[3].buffer.pointer = args;

    err = acpi_evaluate_object(handle, "_DSM", &input, &output);
    if (err) {
        struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER, NULL };

        acpi_get_name(handle, ACPI_FULL_PATHNAME, &buf);

        pr_err("%s: failed to evaluate _DSM command", __func__);

        return err;
    }

    kfree(output.pointer);

    return 0;
}

static int bbswitch_optimus_dsm(acpi_handle handle) {
    char args[] = { 1, 0, 0, 3 };

    if (acpi_call_dsm(handle, acpi_optimus_dsm_muid, 0x100, 0x1A, args)) {
        return 1;
    }

    pr_info("%s: _DSM command evaluated successfully\n", __func__);

    return 0;
}

static int bbswitch_pci_runtime_suspend(struct device *dev) {
    struct pci_dev *pdev = to_pci_dev(dev);
    acpi_handle handle = ACPI_HANDLE(&pdev->dev);

    bbswitch_optimus_dsm(handle);
    pci_save_state(pdev);
    pci_set_power_state(pdev, PCI_D3hot);

    pr_info("%s: suspending dedicated GPU\n", __func__);

    return 0;
}

static int bbswitch_pci_runtime_resume(struct device *dev) {
    pr_info("%s: resuming dedicated GPU\n", __func__);

    return 0;
}

static const struct dev_pm_ops bbswitch_pci_pm_ops = {
    .runtime_suspend = bbswitch_pci_runtime_suspend,
    .runtime_resume = bbswitch_pci_runtime_resume,
};

static int bbswitch_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
    pm_runtime_set_active(&pdev->dev);
    pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
    pm_runtime_use_autosuspend(&pdev->dev);
    pm_runtime_allow(&pdev->dev);
    pm_runtime_put_autosuspend(&pdev->dev);

    return 0;
}

static void bbswitch_pci_remove(struct pci_dev *pdev) {
    pm_runtime_get_noresume(&pdev->dev);
    pm_runtime_dont_use_autosuspend(&pdev->dev);
    pm_runtime_forbid(&pdev->dev);
}

static const struct pci_device_id pciidlist[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID),
            PCI_CLASS_DISPLAY_3D << 8, 0xffff00 },
    { 0, 0, 0 },
};

static struct pci_driver bbswitch_pci_driver = {
    .name = KBUILD_MODNAME,
    .id_table = pciidlist,
    .probe  = bbswitch_pci_probe,
    .remove = bbswitch_pci_remove,
    .driver.pm = &bbswitch_pci_pm_ops,
};

module_pci_driver(bbswitch_pci_driver);

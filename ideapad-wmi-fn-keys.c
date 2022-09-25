// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ideapad-wmi-fn-keys.c - Ideapad WMI fn keys driver
 *
 * Supported models:
 * - Lenovo Yoga 9 14IAP7
 * - Lenovo Yoga 9 14ITL5
 *
 * Copyright (C) 2022 Philipp Jungkamp <p.jungkamp@gmx.net>
 * Copyright (C) 2022 Ulrich Huber <ulrich@huberulrich.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/wmi.h>

#define IDEAPAD_FN_KEY_EVENT_GUID	"8FC0DE0C-B4E4-43FD-B0F3-8871711C1294"

struct ideapad_wmi_private {
	struct wmi_device *wmi_device;
	struct input_dev *input_dev;
};

static const struct key_entry ideapad_wmi_fn_key_keymap[] = {
	/* Customizable Lenovo Hotkey (Acts on Windows as macro key) ("star" with 'S' inside) */
	{ KE_KEY,	0x01, { KEY_MACRO1 } },
	/* Disable FnLock (handled by the firmware) */
	{ KE_IGNORE,	0x02 },
	/* Enable FnLock (handled by the firmware) */
	{ KE_IGNORE,	0x03 },
	/* Snipping (dashed circle with scissors) */
	{ KE_KEY,	0x04, { KEY_SELECTIVE_SCREENSHOT } },
	/* Customizable Lenovo Hotkey ("star" with 'S' inside) (long-press) */
	{ KE_KEY,	0x08, { KEY_FAVORITES } },
	/* Sound profile switch */
	{ KE_KEY,	0x12, { KEY_PROG2 } },
	/* Dark mode toggle */
	{ KE_KEY,	0x13, { KEY_PROG1 } },
	/* Lenovo Support */
	{ KE_KEY,	0x27, { KEY_HELP } },
	/* Lenovo Virtual Background application */
	{ KE_KEY,	0x28, { KEY_PROG3 } },
	{ KE_END },
};

static int ideapad_wmi_input_init(struct ideapad_wmi_private *priv)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (!input_dev) {
		return -ENOMEM;
	}

	input_dev->name = "Ideapad WMI Fn Keys";
	input_dev->phys = IDEAPAD_FN_KEY_EVENT_GUID "/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &priv->wmi_device->dev;

	err = sparse_keymap_setup(input_dev, ideapad_wmi_fn_key_keymap, NULL);
	if (err) {
		dev_err(&priv->wmi_device->dev,
			"Could not set up input device keymap: %d\n", err);
		goto err_free_dev;
	}

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&priv->wmi_device->dev,
			"Could not register input device: %d\n", err);
		goto err_free_dev;
	}

	priv->input_dev = input_dev;
	return 0;

err_free_dev:
	input_free_device(input_dev);
	return err;
}

static void ideapad_wmi_input_exit(struct ideapad_wmi_private *priv)
{
	input_unregister_device(priv->input_dev);
	priv->input_dev = NULL;
}

static void ideapad_wmi_input_report(struct ideapad_wmi_private *priv,
				     unsigned int scancode)
{
    if (!sparse_keymap_report_event(priv->input_dev, scancode, 1, true))
		pr_info("ideapad-wmi-fn-keys: Unknown scancode %x\n", scancode);
}

static int ideapad_wmi_probe(struct wmi_device *wdev, const void *ctx)
{
	struct ideapad_wmi_private *priv;
	int err;

	priv = devm_kzalloc(&wdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, priv);

	priv->wmi_device = wdev;

	err = ideapad_wmi_input_init(priv);
	if (err)
		return err;

	return 0;
}

static void ideapad_wmi_remove(struct wmi_device *wdev)
{
	struct ideapad_wmi_private *priv = dev_get_drvdata(&wdev->dev);

	ideapad_wmi_input_exit(priv);
}

static void ideapad_wmi_notify(struct wmi_device *wdev, union acpi_object *data)
{
	struct ideapad_wmi_private *priv = dev_get_drvdata(&wdev->dev);

	if(data->type != ACPI_TYPE_INTEGER) {
		dev_warn(&priv->wmi_device->dev,
			"WMI event data is not an integer\n");
		return;
	}

	ideapad_wmi_input_report(priv, data->integer.value);
}

static const struct wmi_device_id ideapad_wmi_id_table[] = {
	{	/* Special Keys on the Yoga 9 14IAP7 */
		.guid_string = IDEAPAD_FN_KEY_EVENT_GUID
	},
	{ }
};

static struct wmi_driver ideapad_wmi_driver = {
	.driver = {
		.name = "ideapad-wmi-fn-keys",
	},
	.id_table = ideapad_wmi_id_table,
	.probe = ideapad_wmi_probe,
	.remove = ideapad_wmi_remove,
	.notify = ideapad_wmi_notify,
};

module_wmi_driver(ideapad_wmi_driver);

MODULE_DEVICE_TABLE(wmi, ideapad_wmi_id_table);
MODULE_AUTHOR("Ulrich Huber <ulrich@huberulrich.de>");
MODULE_DESCRIPTION("Ideapad WMI fn keys driver");
MODULE_LICENSE("GPL");

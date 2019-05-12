/* shirda_dt_pinctrl.c (Infrared driver module)
 *
 * Copyright (C) 2009-2010 SHARP CORPORATION All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include	<linux/of_gpio.h>
#include	<linux/of_device.h>

#define	SHIRDA_PINCTRL_OF_IRDA "qcom,shirda"

#define	SHIRDA_SD_NAME			"irda,sd-gpio"
#define	SHIRDA_TX_NAME			"irda,tx-gpio"
#define	SHIRDA_RX_NAME			"irda,rx-gpio"

#define	PINCTRL_STATE_INIT		"default"
#define	PINCTRL_STATE_TXRX_GPIO		"sleep"
#define	PINCTRL_STATE_TXRX_UART		"active"
#define	PINCTRL_STATE_TX_GPIO		"rx-active"
#define	PINCTRL_STATE_RX_GPIO		"tx-active"

typedef enum {
	SHIRDA_IDLE,
	SHIRDA_OUTPUT,
	SHIRDA_INPUT,
	SHIRDA_FUNC_UART,
} shirda_gpio_iocnf;

typedef enum {
	SHIRDA_PINCTRL_STATE_INIT = 0,
	SHIRDA_PINCTRL_STATE_TXRX_GPIO,
	SHIRDA_PINCTRL_STATE_TXRX_UART,
	SHIRDA_PINCTRL_STATE_TX_GPIO,
	SHIRDA_PINCTRL_STATE_RX_GPIO,
	SHIRDA_PINCTRL_STATE_MAX
} shirda_pinctrl_state_number;

typedef enum {
	SHIRDA_GPIO_SD_ID = 0,
	SHIRDA_GPIO_TX_ID,
	SHIRDA_GPIO_RX_ID,
	SHIRDA_USE_GPIO_MAX
} shirda_use_gpio_number;

typedef enum {
	SHIRDA_GPIO_STATE_IDLE = 0,
	SHIRDA_GPIO_STATE_MODE_SET,
	SHIRDA_GPIO_STATE_UART,
	SHIRDA_GPIO_STATE_TX,
	SHIRDA_GPIO_STATE_MAX
} shirda_gpio_state_number;

struct shirda_pinctrl {
	struct pinctrl		*p;

	struct pinctrl_state	*statep[SHIRDA_PINCTRL_STATE_MAX];

	int			gpio[SHIRDA_USE_GPIO_MAX];

};

struct shirda_pinctrl	shirda_gpdata;

struct shirda_pinctrl_state {
	struct pinctrl_state	**sp;
	char		name[16];
	char		label[32];
};

const struct shirda_pinctrl_state shirda_pinctrl_tbl[] = {
	{
		.sp = &shirda_gpdata.statep[SHIRDA_PINCTRL_STATE_INIT],
		.name = PINCTRL_STATE_INIT,
		.label = "tx-gpio, rx-uart",
	},
	{
		.sp = &shirda_gpdata.statep[SHIRDA_PINCTRL_STATE_TXRX_GPIO],
		.name = PINCTRL_STATE_TXRX_GPIO,
		.label = "tx-gpio, rx-gpio",
	},
	{
		.sp = &shirda_gpdata.statep[SHIRDA_PINCTRL_STATE_TXRX_UART],
		.name = PINCTRL_STATE_TXRX_UART,
		.label = "tx-uart, rx-uart",
	},
	{
		.sp = &shirda_gpdata.statep[SHIRDA_PINCTRL_STATE_TX_GPIO],
		.name = PINCTRL_STATE_TX_GPIO,
		.label = "tx-gpio, rx-uart",
	},
	{
		.sp = &shirda_gpdata.statep[SHIRDA_PINCTRL_STATE_RX_GPIO],
		.name = PINCTRL_STATE_RX_GPIO,
		.label = "tx-uart, rx-gpio",
	},
};

struct shirda_gpio {
	int	*gpio;
	int	iocfg[SHIRDA_GPIO_STATE_MAX];
	int	val[SHIRDA_GPIO_STATE_MAX];
	char	name[16];
	char	label[16];
};

const struct shirda_gpio shirda_gpio_tbl[] = {
	{
		.gpio	= &shirda_gpdata.gpio[SHIRDA_GPIO_SD_ID],
		.iocfg = {
				SHIRDA_OUTPUT,
				SHIRDA_OUTPUT,
				SHIRDA_OUTPUT,
				SHIRDA_OUTPUT,
			},
		.val	= {
				SHIRDA_SD_SHUTDOWN,
				SHIRDA_SD_ACTIVE,
				SHIRDA_SD_ACTIVE,
				SHIRDA_SD_ACTIVE,
			},
		.name	= SHIRDA_SD_NAME,
		.label	= "IRDA_SD",
	},
	{
		.gpio	= &shirda_gpdata.gpio[SHIRDA_GPIO_TX_ID],
		.iocfg = {
				SHIRDA_OUTPUT,
				SHIRDA_OUTPUT,
				SHIRDA_FUNC_UART,
				SHIRDA_FUNC_UART,
			},
		.val	= {
				SHIRDA_GPIO_LOW,
				SHIRDA_GPIO_LOW,
				SHIRDA_GPIO_IDLE,
				SHIRDA_GPIO_IDLE,
			},
		.name	= SHIRDA_TX_NAME,
		.label	= "IRDA_TX",
	},
	{
		.gpio	= &shirda_gpdata.gpio[SHIRDA_GPIO_RX_ID],
		.iocfg = {
				SHIRDA_INPUT,
				SHIRDA_FUNC_UART,
				SHIRDA_FUNC_UART,
				SHIRDA_INPUT,
			},
		.val	= {
				SHIRDA_GPIO_IDLE,
				SHIRDA_GPIO_IDLE,
				SHIRDA_GPIO_IDLE,
				SHIRDA_GPIO_IDLE,
			},
		.name	= SHIRDA_RX_NAME,
		.label	= "IRDA_RX",
	},
};

#define	SHIRDA_GPIO_SD	(*(shirda_gpio_tbl[SHIRDA_GPIO_SD_ID].gpio))
#define	SHIRDA_GPIO_TXD	(*(shirda_gpio_tbl[SHIRDA_GPIO_TX_ID].gpio))
#define	SHIRDA_GPIO_RXD	(*(shirda_gpio_tbl[SHIRDA_GPIO_RX_ID].gpio))

#ifdef	SHIRDA_DEBUG_GPIO_INIT
typedef enum {
	SHIRDA_GPIOS_NON = 0,
	SHIRDA_GPIOS_ACQ,
	SHIRDA_GPIOS_SET,
	SHIRDA_GPIOS_ENA,
	SHIRDA_GPIOS_ALL,
	SHIRDA_GPIOS_DIS
} shirda_gpios_enum;
static shirda_gpios_enum shirda_gpios = SHIRDA_GPIOS_NON;
#endif

static int shirda_gpios_request(struct platform_device *pdev);
static int shirda_gpios_direction(shirda_gpio_state_number s);
static void shirda_gpio_free(void);
static void shirda_gpios_disable(void);

static int shirda_gpios_request(struct platform_device *pdev)
{
	int	ret, gpio, i;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	for (i = 0; i < SHIRDA_USE_GPIO_MAX; i++) {
		gpio = of_get_named_gpio(pdev->dev.of_node,
						shirda_gpio_tbl[i].name, 0);
		if (gpio_is_valid(gpio)) {
#ifdef	SHIRDA_DEBUG_GPIO_INIT
			IRDALOG_INFO("%s: %s get name success\n",
						shirda_gpio_tbl[i].label,
						shirda_gpio_tbl[i].name);
#endif
			ret = gpio_request(gpio, shirda_gpio_tbl[i].label);
			if (ret != 0) {
				IRDALOG_FATAL(
					"gpio_request(%d) <%s> failed: %d\n",
					gpio, shirda_gpio_tbl[i].label, ret);
				return ret;
			}
			*(shirda_gpio_tbl[i].gpio) = gpio;
#ifdef	SHIRDA_DEBUG_GPIO_INIT
			IRDALOG_INFO("%s: gpio_request(%d) success\n",
					shirda_gpio_tbl[i].label, gpio);
#endif
		} else {
			IRDALOG_FATAL("%s: get name gpio(%s) failed %d\n",
						shirda_gpio_tbl[i].label,
						shirda_gpio_tbl[i].name, gpio);
			shirda_gpio_free();
			return -EIO;
		}
	}
	return 0;
}

static int shirda_gpios_direction(shirda_gpio_state_number s)
{
	int			ret = 0;
	int			i;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry %d\n", s);
#endif

	if (s < 0 || SHIRDA_GPIO_STATE_MAX <= s) {
		IRDALOG_FATAL("gpio state error %d\n", s);
		return -EIO;
	}

	for (i = 0; i < SHIRDA_USE_GPIO_MAX; i++) {
		if (*(shirda_gpio_tbl[i].gpio) == 0) {
			IRDALOG_FATAL("%s is uncntrollable\n",
						shirda_gpio_tbl[i].label);
			return -EIO;
		}

		if (shirda_gpio_tbl[i].iocfg[s] == SHIRDA_OUTPUT) {
			ret = gpio_direction_output(*(shirda_gpio_tbl[i].gpio),
						shirda_gpio_tbl[i].val[s]);
#ifdef	SHIRDA_DEBUG_GPIO_INIT
			IRDALOG_INFO("%s: gpio %d direction output [%d]\n",
						shirda_gpio_tbl[i].label,
						*(shirda_gpio_tbl[i].gpio),
						shirda_gpio_tbl[i].val[s]);
#endif
		} else if (shirda_gpio_tbl[i].iocfg[s] == SHIRDA_INPUT) {
			ret = gpio_direction_input(*(shirda_gpio_tbl[i].gpio));
#ifdef	SHIRDA_DEBUG_GPIO_INIT
			IRDALOG_INFO("%s: gpio %d direction input\n",
						shirda_gpio_tbl[i].label,
						*(shirda_gpio_tbl[i].gpio));
#endif
		}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
		else {
			IRDALOG_INFO("%s: gpio not set\n",
						shirda_gpio_tbl[i].label);
		}
#endif
		if (ret != 0) {
			IRDALOG_FATAL("%s: gpio %d direction error %d\n",
						shirda_gpio_tbl[i].label,
						*(shirda_gpio_tbl[i].gpio),
									ret);
			return -EIO;
		}
	}
	return 0;
}

static void shirda_gpio_free(void)
{
	int	ret, i;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	ret = shirda_gpios_direction(SHIRDA_GPIO_STATE_IDLE);
	if (ret != 0) {
		IRDALOG_FATAL("direction error %d\n", ret);
	}
	ret = pinctrl_select_state(shirda_gpdata.p,
			*(shirda_pinctrl_tbl[SHIRDA_PINCTRL_STATE_INIT].sp));
	if (ret != 0) {
		IRDALOG_FATAL("select state error %d\n", ret);
	}

	for (i = 0; i < SHIRDA_USE_GPIO_MAX; i++) {
		if (*(shirda_gpio_tbl[i].gpio) != 0) {
			gpio_free(*(shirda_gpio_tbl[i].gpio));
#ifdef	SHIRDA_DEBUG_GPIO_INIT
			IRDALOG_INFO("%s, %s gpio %d free\n",
						shirda_gpio_tbl[i].label,
						shirda_gpio_tbl[i].name,
						*(shirda_gpio_tbl[i].gpio));
#endif
		}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
		else {
			IRDALOG_INFO("%s, %s gpio number is zero\n",
						shirda_gpio_tbl[i].label,
						shirda_gpio_tbl[i].name);
		}
#endif
	}
	return;
}

static int shirda_gpio_init(struct platform_device *pdev)
{
	struct shirda_pinctrl	*dp = &shirda_gpdata;
	struct pinctrl		*pin;
	int			ret = 0;
#ifndef	SHIRDA_DEBUG_GPIO_INIT
	struct pinctrl_state	*pinsp;
	int			i;
#endif

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	if (pdev->dev.of_node == NULL) {
		IRDALOG_FATAL("node address error\n");
		return -EIO;
	}

	memset(&shirda_gpdata, 0x00, sizeof(shirda_gpdata));

	pin = devm_pinctrl_get(&(pdev->dev));
	if (IS_ERR(pin)) {
		ret = PTR_ERR(pin);
		IRDALOG_FATAL("pinctrl get (address:0x%lx) error %d\n",
						(unsigned long)pin, ret);
		return ret;
	}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	else {
		IRDALOG_INFO("pinctrl get success (address:0x%lx)\n",
							(unsigned long)pin);
	}
#endif
	dp->p = pin;
#ifndef	SHIRDA_DEBUG_GPIO_INIT
	for (i = 0; i < (int)SHIRDA_PINCTRL_STATE_MAX; i++) {
		pinsp = pinctrl_lookup_state(dp->p,
						shirda_pinctrl_tbl[i].name);
		if (IS_ERR(pinsp)) {
			ret = PTR_ERR(pinsp);
			IRDALOG_FATAL("%s(%s) pinctrl lookup error %d\n",
						shirda_pinctrl_tbl[i].label,
						shirda_pinctrl_tbl[i].name,
									ret);
			return ret;
		}
		dp->statep[i] = pinsp;
#ifdef	SHIRDA_DEBUG_GPIO_INIT
		IRDALOG_INFO("%s(%s) pinctrl lookup success\n",
						shirda_pinctrl_tbl[i].label,
						shirda_pinctrl_tbl[i].name);
		IRDALOG_INFO("pinctrl address 0x%lx == 0x%lx\n",
					(long)dp->statep[i],
					(long)*(shirda_pinctrl_tbl[i].sp));
#endif
	}
#endif
	ret = shirda_gpios_request(pdev);
	if (ret != 0)	return ret;
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	shirda_gpios = SHIRDA_GPIOS_ACQ;
	IRDALOG_INFO(" gpios_request done. shirda_gpios = %d\n", shirda_gpios);
#endif

	ret = shirda_gpios_direction(SHIRDA_GPIO_STATE_IDLE);
	if (ret != 0) {
		shirda_gpio_free();
		return ret;
	}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	shirda_gpios = SHIRDA_GPIOS_SET;
	IRDALOG_INFO(" gpios_direction done. shirda_gpios = %d\n",
								 shirda_gpios);
#endif

	ret = pinctrl_select_state(dp->p,
					dp->statep[SHIRDA_PINCTRL_STATE_INIT]);
	if (ret != 0) {
		IRDALOG_FATAL("pinctrl select error %d", ret);
	}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	else {
		shirda_gpios = SHIRDA_GPIOS_ENA;
		IRDALOG_INFO(" gpios_enable done. shirda_gpios = %d\n",
								shirda_gpios);
	}
#endif

	return ret;
}

static void shirda_gpios_disable(void)
{
	int	ret;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	ret = shirda_gpios_direction(SHIRDA_GPIO_STATE_IDLE);
	if (ret != 0) {
		IRDALOG_FATAL("gpio direction error %d\n", ret);
		return;
	}

	if (shirda_gpdata.p == NULL ||
	    *(shirda_pinctrl_tbl[SHIRDA_PINCTRL_STATE_INIT].sp) == NULL) {
		IRDALOG_FATAL("pinctrl gpio is uncntrollable\n");
		return;
	}

	ret = pinctrl_select_state(shirda_gpdata.p,
			*(shirda_pinctrl_tbl[SHIRDA_PINCTRL_STATE_INIT].sp));
	if (ret != 0) {
		IRDALOG_FATAL("gpio select state error %d\n", ret);
	}

	return;
}

static int shirda_gpios_enable(shirda_gpio_mode mode)
{
	int	ret, pret;
	shirda_gpio_state_number	g;
	shirda_pinctrl_state_number	p;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry mode=%d\n", (int)mode);
#endif

	if (shirda_gpdata.p == NULL) {
		IRDALOG_FATAL("pinctrl gpio is uncntrollable\n");
		goto	gpio_error;
	}

	switch (mode) {
	case SHIRDA_GPIO_MODE_IDLE:
		g = SHIRDA_GPIO_STATE_IDLE;
		p = SHIRDA_PINCTRL_STATE_INIT;
		break;
	case SHIRDA_GPIO_MODE_ACTIVE:
		g = SHIRDA_GPIO_STATE_UART;
		p = SHIRDA_PINCTRL_STATE_TXRX_UART;
		break;
	case SHIRDA_GPIO_MODE_ACTIVE2:
		g = SHIRDA_GPIO_STATE_TX;
		p = SHIRDA_PINCTRL_STATE_RX_GPIO;
		break;
	default:
		IRDALOG_FATAL("Invalid mode %d\n", mode);
		goto	gpio_error;
	}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	IRDALOG_INFO("gpio state no = %d, pinctrl state no = %d\n",
							(int)g, (int)p);
#endif
#ifndef SHIRDA_DEBUG_GPIO_INIT
	if (*(shirda_pinctrl_tbl[p].sp) == NULL) {
		IRDALOG_FATAL("pinctrl state %d is not active\n", (int)p);
		goto	gpio_error;
	}
#endif
	ret = shirda_gpios_direction(g);
	if (ret != 0) {
		IRDALOG_FATAL("gpio mode %d set direction error %d\n",
								mode, ret);
		goto	gpio_error;
	}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	else {
		IRDALOG_INFO("gpio direction success\n");
	}
#endif

	pret = pinctrl_select_state(shirda_gpdata.p,
						*(shirda_pinctrl_tbl[p].sp));
	if (pret != 0) {
		IRDALOG_FATAL("mode=%d, %s : %s gpio select state error %d\n",
					mode, shirda_pinctrl_tbl[p].name,
					shirda_pinctrl_tbl[p].label, pret);
		goto	gpio_error;
	}
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	else {
		IRDALOG_INFO("mode=%d, %s : %s pinctrl select state success\n",
						mode,
						shirda_pinctrl_tbl[p].name,
						shirda_pinctrl_tbl[p].label);
	}
#endif

	return ret;

gpio_error:
	shirda_gpios_disable();
	return -EIO;
}


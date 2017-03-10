/*
 * drivers/usb/gadget/s3c_udc_otg.c
 * Samsung S3C on-chip full/high speed USB OTG 2.0 device controllers
 *
 * Copyright (C) 2008 for Samsung Electronics
 * Copyright (C) 2014 for Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/usb/phy.h>

#include <plat/cpu.h>

#include "regs-otg.h"
#include "udc-hs.h"
#include "s3c_udc.h"

#undef DEBUG_S3C_UDC_SETUP
#undef DEBUG_S3C_UDC_EP0
#undef DEBUG_S3C_UDC_ISR
#undef DEBUG_S3C_UDC_OUT_EP
#undef DEBUG_S3C_UDC_IN_EP
#undef DEBUG_S3C_UDC

#define EP0_CON		0
#define EP1_OUT		1
#define EP2_IN		2
#define EP3_IN		3
#define EP_MASK		0xF

#define POLL_TIMEOUT	1000

#define BACK2BACK_SIZE	4

#if defined(DEBUG_S3C_UDC_SETUP) || defined(DEBUG_S3C_UDC_ISR)\
	|| defined(DEBUG_S3C_UDC_OUT_EP)

static char *state_names[] = {
	"WAIT_FOR_SETUP",
	"DATA_STATE_XMIT",
	"DATA_STATE_NEED_ZLP",
	"WAIT_FOR_OUT_STATUS",
	"DATA_STATE_RECV",
	};
#endif

#ifdef DEBUG_S3C_UDC_SETUP
#define DEBUG_SETUP(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_SETUP(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_EP0
#define DEBUG_EP0(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_EP0(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC
#define DEBUG_UDC(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_UDC(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_ISR
#define DEBUG_ISR(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_ISR(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_OUT_EP
#define DEBUG_OUT_EP(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_OUT_EP(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_IN_EP
#define DEBUG_IN_EP(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_IN_EP(fmt, args...) do {} while (0)
#endif

#define	DRIVER_DESC	"S3C HS USB OTG Device Driver,"\
				"(c) 2008-2009 Samsung Electronics"
#define	DRIVER_VERSION	"15 March 2009"

enum s5p_usb_phy_type {
	S5P_USB_PHY_DEVICE,
	S5P_USB_PHY_HOST,
	S5P_USB_PHY_DRD,
};

struct s3c_udc	*the_controller;

static const char driver_name[] = "s3c-hsotg";
static const char driver_desc[] = DRIVER_DESC;
static const char ep0name[] = "ep0-control";

/* Max packet size*/
static unsigned int ep0_fifo_size = 64;
static unsigned int ep_fifo_size =  512;
static unsigned int ep_fifo_size2 = 1024;
static int reset_available = 1;
static int pullup_state;

/*
  Local declarations.
*/
static int s3c_ep_enable(struct usb_ep *ep,
				const struct usb_endpoint_descriptor *);
static int s3c_ep_disable(struct usb_ep *ep);
static struct usb_request *s3c_alloc_request(struct usb_ep *ep,
							gfp_t gfp_flags);
static void s3c_free_request(struct usb_ep *ep, struct usb_request *);

static int s3c_queue(struct usb_ep *ep, struct usb_request *, gfp_t gfp_flags);
static int s3c_dequeue(struct usb_ep *ep, struct usb_request *);
static int s3c_fifo_status(struct usb_ep *ep);
static void s3c_fifo_flush(struct usb_ep *ep);
static void s3c_ep0_read(struct s3c_udc *udc);
static void s3c_ep0_kick(struct s3c_udc *udc, struct s3c_ep *ep);
static void s3c_handle_ep0(struct s3c_udc *udc);
static int s3c_ep0_write(struct s3c_udc *udc);
static int write_fifo_ep0(struct s3c_ep *ep, struct s3c_request *req);
static void done(struct s3c_ep *ep,
			struct s3c_request *req, int status);
static void stop_activity(struct s3c_udc *udc,
			struct usb_gadget_driver *driver);
static int s3c_udc_enable(struct s3c_udc *udc);
static void s3c_udc_disable(struct s3c_udc *udc);
static void udc_set_address(struct s3c_udc *udc, unsigned char address);
static void reset_usbd(void);
static void reconfig_usbd(void);
static void set_max_pktsize(struct s3c_udc *udc, enum usb_device_speed speed);
static void nuke(struct s3c_ep *ep, int status);
static int s3c_udc_set_halt(struct usb_ep *_ep, int value);
static void s3c_udc_soft_connect(void);
static void s3c_udc_soft_disconnect(void);

static struct usb_ep_ops s3c_ep_ops = {
	.enable = s3c_ep_enable,
	.disable = s3c_ep_disable,

	.alloc_request = s3c_alloc_request,
	.free_request = s3c_free_request,

	.queue = s3c_queue,
	.dequeue = s3c_dequeue,

	.set_halt = s3c_udc_set_halt,
	.fifo_status = s3c_fifo_status,
	.fifo_flush = s3c_fifo_flush,
};

#ifdef CONFIG_USB_GADGET_DEBUG_FILES

static const char proc_node_name[] = "driver/udc";

static int
udc_proc_read(char *page, char **start, off_t off, int count,
	      int *eof, void *_dev)
{
	char *buf = page;
	struct s3c_udc *dev = _dev;
	char *next = buf;
	unsigned size = count;
	unsigned long flags;
	int t;

	if (off != 0)
		return 0;

	local_irq_save(flags);

	/* basic device status */
	t = scnprintf(next, size,
		      DRIVER_DESC "\n"
		      "%s version: %s\n"
		      "Gadget driver: %s\n"
		      "\n",
		      driver_name, DRIVER_VERSION,
		      dev->driver ? dev->driver->driver.name : "(none)");
	size -= t;
	next += t;

	local_irq_restore(flags);
	*eof = 1;
	return count - size;
}

#define create_proc_files() \
	create_proc_read_entry(proc_node_name, 0, NULL, udc_proc_read, dev)
#define remove_proc_files() \
	remove_proc_entry(proc_node_name, NULL)

#else	/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

#endif	/* CONFIG_USB_GADGET_DEBUG_FILES */

#include "s3c_udc_otg_xfer_dma.c"

/*
 * is_nonswitch - whether or not switch driver.
 *
 * Return true if switch driver isn't.
 */
static inline bool is_nonswitch(void)
{
#if defined(CONFIG_USB_GADGET_SWITCH) || defined(CONFIG_USB_EXYNOS_SWITCH)
	return false;
#else
	return true;
#endif
}

/*
 *	s3c_udc_disable - disable USB device controller
 */
static void s3c_udc_disable(struct s3c_udc *udc)
{
	struct platform_device *pdev = udc->dev;
	struct s3c_hsotg_plat *pdata = pdev->dev.platform_data;
	u32 utemp;
	DEBUG_SETUP("%s: %p\n", __func__, udc);

	disable_irq(udc->irq);
	udc_set_address(udc, 0);

	udc->ep0state = WAIT_FOR_SETUP;
	udc->gadget.speed = USB_SPEED_UNKNOWN;
	udc->usb_address = 0;

	/* Mask the core interrupt */
	__raw_writel(0, udc->regs + S3C_UDC_OTG_GINTMSK);

	/* Put the OTG device core in the disconnected state.*/
	utemp = __raw_readl(udc->regs + S3C_UDC_OTG_DCTL);
	utemp |= SOFT_DISCONNECT;
	__raw_writel(utemp, udc->regs + S3C_UDC_OTG_DCTL);
	udelay(20);

	if (udc->phy)
		usb_phy_shutdown(udc->phy);
	else if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, S5P_USB_PHY_DEVICE);

	clk_disable_unprepare(udc->clk);
}

/*
 *	s3c_udc_reinit - initialize software state
 */
static void s3c_udc_reinit(struct s3c_udc *udc)
{
	unsigned int i;

	DEBUG_SETUP("%s: %p\n", __func__, udc);

	/* device/ep0 records init */
	INIT_LIST_HEAD(&udc->gadget.ep_list);
	INIT_LIST_HEAD(&udc->gadget.ep0->ep_list);
	udc->ep0state = WAIT_FOR_SETUP;

	/* basic endpoint records init */
	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		struct s3c_ep *ep = &udc->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &udc->gadget.ep_list);

		ep->desc = 0;
		ep->stopped = 0;
		INIT_LIST_HEAD(&ep->queue);
		ep->pio_irqs = 0;
	}

	/* the rest was statically initialized, and is read-only */
}

#define BYTES2MAXP(x)	(x / 8)
#define MAXP2BYTES(x)	(x * 8)

/* until it's enabled, this UDC should be completely invisible
 * to any USB host.
 */
static int s3c_udc_enable(struct s3c_udc *udc)
{
	struct platform_device *pdev = udc->dev;
	struct s3c_hsotg_plat *pdata = pdev->dev.platform_data;
	DEBUG_SETUP("%s: %p\n", __func__, udc);
	enable_irq(udc->irq);
	clk_prepare_enable(udc->clk);

	if (udc->phy)
                usb_phy_init(udc->phy);
        else if (pdata->phy_init)
                pdata->phy_init(pdev, S5P_USB_PHY_DEVICE);

	reconfig_usbd();

	DEBUG_SETUP("S3C USB 2.0 OTG Controller Core Initialized : 0x%x\n",
			__raw_readl(udc->regs + S3C_UDC_OTG_GINTMSK));

	udc->gadget.speed = USB_SPEED_UNKNOWN;

	return 0;
}

int s3c_vbus_enable(struct usb_gadget *gadget, int is_active)
{
	unsigned long flags;
	struct s3c_udc *udc = container_of(gadget, struct s3c_udc, gadget);
	if (udc->udc_enabled != is_active) {
		udc->udc_enabled = is_active;

		if (!is_active) {
			spin_lock_irqsave(&udc->lock, flags);
			stop_activity(udc, udc->driver);
			spin_unlock_irqrestore(&udc->lock, flags);
			s3c_udc_disable(udc);
		} else {
			s3c_udc_reinit(udc);
			s3c_udc_enable(udc);
			if (pullup_state)
				s3c_udc_soft_connect();
		}
	} else {
		printk(KERN_INFO "usb: %s, udc_enabled : %d, is_active : %d\n",
				__func__, udc->udc_enabled, is_active);
	}

	return 0;
}

/*
  Register entry point for the peripheral controller driver.
*/
static int s3c_udc_start(struct usb_gadget *gadget,
			 struct usb_gadget_driver *driver)
{
	struct s3c_udc *udc = the_controller;

	DEBUG_SETUP("%s: %s\n", __func__, driver->driver.name);

	if (!driver
		|| driver->max_speed < USB_SPEED_FULL
		|| !driver->disconnect || !driver->setup)
		return -EINVAL;
	if (!udc)
		return -ENODEV;
	if (udc->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	udc->driver = driver;
	udc->gadget.dev.driver = &driver->driver;

	printk(KERN_INFO "bound driver '%s'\n",
			driver->driver.name);
	if (is_nonswitch()) {
		s3c_udc_enable(udc);
		udc->udc_enabled = 1;
	} else {
		printk(KERN_INFO "usb: Skip udc_enable\n");
	}

	return 0;
}

/*
  Unregister entry point for the peripheral controller driver.
*/
static int s3c_udc_stop(struct usb_gadget *gadget,
			struct usb_gadget_driver *driver)
{
	struct s3c_udc *udc = the_controller;
	unsigned long flags;

	if (!udc)
		return -ENODEV;
	if (!driver || driver != udc->driver)
		return -EINVAL;

	spin_lock_irqsave(&udc->lock, flags);
	udc->driver = NULL;
	stop_activity(udc, driver);
	spin_unlock_irqrestore(&udc->lock, flags);

	if (is_nonswitch())
		s3c_udc_disable(udc);

	printk(KERN_INFO "Unregistered gadget driver '%s'\n",
			driver->driver.name);

	return 0;
}

static int poll_bit_set(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = __raw_readl(ptr);
		if (reg & val)
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

static int poll_bit_clear(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = __raw_readl(ptr);
		if (!(reg & val))
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

/*
 *	done - retire a request; caller blocked irqs
 */
static void done(struct s3c_ep *ep, struct s3c_request *req, int status)
{
	unsigned int stopped = ep->stopped;
	struct device *dev = &the_controller->dev->dev;
	u32 ep_num = ep_index(ep);
	struct s3c_udc *udc = ep->dev;
	u32 ctrl, ret;

	DEBUG_UDC("%s: %s %p, req = %p, stopped = %d\n",
		__func__, ep->ep.name, ep, &req->req, stopped);

	list_del_init(&req->queue);

	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (req->mapped) {
		if (ep_is_in(ep)) {
			ctrl = __raw_readl(udc->regs + S3C_UDC_OTG_DIEPCTL(ep_num));
			if (ctrl & DEPCTL_EPENA) {
				__raw_writel(DEPCTL_EPDIS | DEPCTL_SNAK | ctrl,
					udc->regs + S3C_UDC_OTG_DIEPCTL(ep_num));
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_DIEPCTL(ep_num),
						DEPCTL_EPENA | DEPCTL_EPDIS, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "faild Endpoint disable and Set NAK\n");
					goto out;
				}

				__raw_writel(TxFIFONum(ep_num) | TxFFlush, udc->regs + S3C_UDC_OTG_GRSTCTL);
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_GRSTCTL,
						TxFFlush, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "faild TX FIFO Flush\n");
					goto out;
				}
			}
		} else {
			ctrl =  __raw_readl(udc->regs + S3C_UDC_OTG_DOEPCTL(ep_num));
			if (ctrl & DEPCTL_EPENA) {
				__raw_writel(__raw_readl(udc->regs + S3C_UDC_OTG_DCTL) | SGOUTNak,
						udc->regs + S3C_UDC_OTG_DCTL);
				ret = poll_bit_set(udc->regs + S3C_UDC_OTG_GINTSTS,
						INT_GOUTNakEff, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "failed Set Global OUT NAK\n");
					goto out;
				}

				__raw_writel(DEPCTL_EPDIS | DEPCTL_SNAK | ctrl,
					udc->regs + S3C_UDC_OTG_DOEPCTL(ep_num));
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_DOEPCTL(ep_num),
						DEPCTL_EPENA | DEPCTL_EPDIS, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "faild Endpoint disable and Set NAK\n");
					goto out;
				}

				__raw_writel(__raw_readl(udc->regs + S3C_UDC_OTG_DCTL) | CGOUTNak,
						udc->regs + S3C_UDC_OTG_DCTL);
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_GINTSTS,
						INT_GOUTNakEff, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "failed Clear Global OUT NAK\n");
					goto out;
				}
			}
		}
out:
		dma_unmap_single(dev, req->req.dma, req->req.length,
			(ep->bEndpointAddress & USB_DIR_IN) ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE);
		req->req.dma = DMA_ADDR_INVALID;
		req->mapped = 0;
		aligned_unmap_buf(req, ep_is_in(ep));
	}

	if (status && status != -ESHUTDOWN) {
		DEBUG_UDC("complete %s req %p stat %d len %u/%u\n",
			ep->ep.name, &req->req, status,
			req->req.actual, req->req.length);
	}

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;

	spin_unlock(&ep->dev->lock);
	req->req.complete(&ep->ep, &req->req);
	spin_lock(&ep->dev->lock);

	ep->stopped = stopped;
}

/*
 *	nuke - dequeue ALL requests
 */
static void nuke(struct s3c_ep *ep, int status)
{
	struct s3c_request *req;

	DEBUG_UDC("%s: %s %p\n", __func__, ep->ep.name, ep);

	/* called with irqs blocked */
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct s3c_request, queue);
		done(ep, req, status);
	}
}

static void stop_activity(struct s3c_udc *udc,
			  struct usb_gadget_driver *driver)
{
	int i;

	/* don't disconnect drivers more than once */
	if (udc->gadget.speed == USB_SPEED_UNKNOWN)
		driver = 0;
	udc->gadget.speed = USB_SPEED_UNKNOWN;

	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		struct s3c_ep *ep = &udc->ep[i];
		ep->stopped = 1;
		nuke(ep, -ESHUTDOWN);
	}

	/* report disconnect; the driver is already quiesced */
	if (driver) {
		spin_unlock(&udc->lock);
		driver->disconnect(&udc->gadget);
		spin_lock(&udc->lock);
	}

	/* re-init driver-visible data structures */
	s3c_udc_reinit(udc);
}

static void reset_usbd(void)
{
	struct s3c_udc *udc = the_controller;
	unsigned int utemp;
#ifdef DED_TX_FIFO
	int i;

	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		utemp = __raw_readl(udc->regs + S3C_UDC_OTG_DIEPCTL(i));

		if (utemp & DEPCTL_EPENA)
			__raw_writel(utemp|DEPCTL_EPDIS|DEPCTL_SNAK,
					udc->regs + S3C_UDC_OTG_DIEPCTL(i));

		/* OUT EP0 cannot be disabled */
		if (i != 0) {
			utemp = __raw_readl(udc->regs + S3C_UDC_OTG_DOEPCTL(i));

			if (utemp & DEPCTL_EPENA)
				__raw_writel(utemp|DEPCTL_EPDIS|DEPCTL_SNAK,
				udc->regs + S3C_UDC_OTG_DOEPCTL(i));
		}
	}
#endif

	/* 5. Configure OTG Core to initial settings of device mode.*/
	__raw_writel(1<<18|0x0<<0,
			udc->regs + S3C_UDC_OTG_DCFG);

	mdelay(1);

	/* 6. Unmask the core interrupts*/
	__raw_writel(GINTMSK_INIT, udc->regs + S3C_UDC_OTG_GINTMSK);

	/* 7. Set NAK bit of OUT EP0*/
	__raw_writel(DEPCTL_SNAK,
			udc->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));

	/* 8. Unmask EPO interrupts*/
	__raw_writel(((1<<EP0_CON)<<DAINT_OUT_BIT)|(1<<EP0_CON),
			udc->regs + S3C_UDC_OTG_DAINTMSK);

	/* 9. Unmask device OUT EP common interrupts*/
	__raw_writel(DOEPMSK_INIT, udc->regs + S3C_UDC_OTG_DOEPMSK);

	/* 10. Unmask device IN EP common interrupts*/
	__raw_writel(DIEPMSK_INIT, udc->regs + S3C_UDC_OTG_DIEPMSK);

	/* 11. Set Rx FIFO Size (in 32-bit words) */
	__raw_writel(RX_FIFO_SIZE, udc->regs + S3C_UDC_OTG_GRXFSIZ);

	/* 12. Set Non Periodic Tx FIFO Size*/
	__raw_writel((NPTX_FIFO_SIZE) << 16 | (NPTX_FIFO_START_ADDR) << 0,
		udc->regs + S3C_UDC_OTG_GNPTXFSIZ);

#ifdef DED_TX_FIFO
	for (i = 1; i < S3C_MAX_ENDPOINTS; i++)
		__raw_writel((PTX_FIFO_SIZE) << 16 |
				(NPTX_FIFO_START_ADDR + NPTX_FIFO_SIZE
				  + PTX_FIFO_SIZE*(i-1)) << 0,
				udc->regs + S3C_UDC_OTG_DIEPTXF(i));
#endif

	/* Flush the RX FIFO */
	__raw_writel(0x10, udc->regs + S3C_UDC_OTG_GRSTCTL);
	while (__raw_readl(udc->regs + S3C_UDC_OTG_GRSTCTL) & 0x10)
		;

	/* Flush all the Tx FIFO's */
	__raw_writel(0x10<<6, udc->regs + S3C_UDC_OTG_GRSTCTL);
	__raw_writel((0x10<<6)|0x20, udc->regs + S3C_UDC_OTG_GRSTCTL);
	while (__raw_readl(udc->regs + S3C_UDC_OTG_GRSTCTL) & 0x20)
		;

	/* 13. Clear NAK bit of OUT EP0*/
	/* For Slave mode*/
	__raw_writel(DEPCTL_CNAK,
		udc->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));

	/* 14. Initialize OTG Link Core.*/
	__raw_writel(GAHBCFG_INIT, udc->regs + S3C_UDC_OTG_GAHBCFG);
}

static void reconfig_usbd(void)
{
	struct s3c_udc *udc = the_controller;
	unsigned int utemp;

	/* 2. Soft-reset OTG Core and then unreset again. */
	__raw_writel(CORE_SOFT_RESET, udc->regs + S3C_UDC_OTG_GRSTCTL);

	utemp = (0<<15		/* PHY Low Power Clock sel */
		|1<<14		/* Non-Periodic TxFIFO Rewind Enable */
		|0<<9		/* [0:HNP disable, 1:HNP enable] */
		|0<<8		/* [ 0:SRP disable, 1:SRP enable] */
		|0<<7		/* Ulpi DDR sel */
		|0<<6		/* 0: high speed utmi+, 1: full speed serial */
		|0<<4		/* 0: utmi+, 1:ulpi */
		|0x7<<0);	/* HS/FS Timeout */
	/* [13:10] Turnaround time 0x5:16-bit 0x9:8-bit UTMI+ */
	/* [3] phy i/f 0:8bit, 1:16bit */
	if (soc_is_exynos4210())
		utemp |= (0x5<<10 | 1<<3);
	else
		utemp |= (0x9<<10 | 0<<3);
	__raw_writel(utemp, udc->regs + S3C_UDC_OTG_GUSBCFG);

	/* 3. Put the OTG device core in the disconnected state.*/
	utemp = __raw_readl(udc->regs + S3C_UDC_OTG_DCTL);
	utemp |= SOFT_DISCONNECT;
	__raw_writel(utemp, udc->regs + S3C_UDC_OTG_DCTL);

	reset_usbd();
}

static void set_max_pktsize(struct s3c_udc *udc, enum usb_device_speed speed)
{
	unsigned int ep_ctrl;
	int i;

	if (speed == USB_SPEED_HIGH) {
		ep0_fifo_size = 64;
		ep_fifo_size = 512;
		ep_fifo_size2 = 1024;
		udc->gadget.speed = USB_SPEED_HIGH;
	} else {
		ep0_fifo_size = 64;
		ep_fifo_size = 64;
		ep_fifo_size2 = 1023;
		udc->gadget.speed = USB_SPEED_FULL;
	}

	udc->ep[0].ep.maxpacket = ep0_fifo_size;
	usb_ep_set_maxpacket_limit(&udc->ep[0].ep, ep0_fifo_size);

	for (i = 1; i < S3C_MAX_ENDPOINTS; i++) {
		/* fullspeed limitations don't apply to isochronous endpoints */
		if (udc->ep[i].bmAttributes == USB_ENDPOINT_XFER_ISOC)
			udc->ep[i].ep.maxpacket = ep_fifo_size2;
		else
			udc->ep[i].ep.maxpacket = ep_fifo_size;

		usb_ep_set_maxpacket_limit(&udc->ep[i].ep,
				udc->ep[i].ep.maxpacket);
	}

	/* EP0 - Control IN (64 bytes)*/
	ep_ctrl = __raw_readl(udc->regs + S3C_UDC_OTG_DIEPCTL(EP0_CON));
	__raw_writel(ep_ctrl|(0<<0), udc->regs + S3C_UDC_OTG_DIEPCTL(EP0_CON));

	/* EP0 - Control OUT (64 bytes)*/
	ep_ctrl = __raw_readl(udc->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));
	__raw_writel(ep_ctrl|(0<<0), udc->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));
}

static int s3c_ep_enable(struct usb_ep *_ep,
			     const struct usb_endpoint_descriptor *desc)
{
	struct s3c_ep *ep;
	struct s3c_udc *udc;
	unsigned long flags;

	DEBUG_UDC("%s: %p\n", __func__, _ep);
	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep || !desc || ep->desc || _ep->name == ep0name
	    || desc->bDescriptorType != USB_DT_ENDPOINT
	    || ep->bEndpointAddress != desc->bEndpointAddress
	    || ep_maxpacket(ep) < le16_to_cpu(desc->wMaxPacketSize)) {

		DEBUG_UDC("%s: bad ep or descriptor\n", __func__);
		return -EINVAL;
	}

	/* xfer types must match, except that interrupt ~= bulk */
	if (ep->bmAttributes !=
			(desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
	    && ep->bmAttributes != USB_ENDPOINT_XFER_BULK
	    && desc->bmAttributes != USB_ENDPOINT_XFER_INT) {

		DEBUG_UDC("%s: %s type mismatch\n", __func__, _ep->name);
		return -EINVAL;
	}

	/* hardware _could_ do smaller, but driver doesn't */
	if ((desc->bmAttributes == USB_ENDPOINT_XFER_BULK
	     && le16_to_cpu(desc->wMaxPacketSize) != ep_maxpacket(ep))
	    || !desc->wMaxPacketSize) {

		DEBUG_UDC("%s: bad %s maxpacket\n", __func__, _ep->name);
		return -ERANGE;
	}

	udc = ep->dev;
	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN) {

		DEBUG_UDC("%s: bogus device state\n", __func__);
		return -ESHUTDOWN;
	}

	ep->stopped = 0;
	ep->desc = desc;
	ep->pio_irqs = 0;
	ep->ep.maxpacket = le16_to_cpu(desc->wMaxPacketSize);

	/* Reset halt state */
	s3c_udc_set_halt(_ep, 0);

	spin_lock_irqsave(&ep->dev->lock, flags);
	s3c_udc_ep_activate(ep);
	spin_unlock_irqrestore(&ep->dev->lock, flags);

	DEBUG_UDC("%s: enabled %s, stopped = %d, maxpacket = %d\n",
		__func__, _ep->name, ep->stopped, ep->ep.maxpacket);
	return 0;
}

/** Disable EP
 */
static int s3c_ep_disable(struct usb_ep *_ep)
{
	struct s3c_ep *ep;
	unsigned long flags;

	DEBUG_UDC("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep || !ep->desc) {
		DEBUG_UDC("%s: %s not enabled\n", __func__,
		      _ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* Nuke all pending requests */
	nuke(ep, -ESHUTDOWN);

	ep->desc = 0;
	ep->stopped = 1;

	spin_unlock_irqrestore(&ep->dev->lock, flags);

	DEBUG_UDC("%s: disabled %s\n", __func__, _ep->name);
	return 0;
}

static struct usb_request *s3c_alloc_request(struct usb_ep *ep,
						 gfp_t gfp_flags)
{
	struct s3c_request *req;

	DEBUG_UDC("%s: %s %p\n", __func__, ep->name, ep);

	req = kmalloc(sizeof *req, gfp_flags);
	if (!req)
		return 0;

	memset(req, 0, sizeof *req);
	INIT_LIST_HEAD(&req->queue);
	return &req->req;
}

static void s3c_free_request(struct usb_ep *ep, struct usb_request *_req)
{
	struct s3c_request *req;

	DEBUG_UDC("%s: %p\n", __func__, ep);

	req = container_of(_req, struct s3c_request, req);
	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

/* dequeue JUST ONE request */
static int s3c_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct s3c_ep *ep;
	struct s3c_request *req;
	unsigned long flags;

	DEBUG_UDC("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep || ep->ep.name == ep0name)
		return -EINVAL;

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req) {
		spin_unlock_irqrestore(&ep->dev->lock, flags);
		return -EINVAL;
	}

	done(ep, req, -ECONNRESET);

	spin_unlock_irqrestore(&ep->dev->lock, flags);
	return 0;
}

/** Return bytes in EP FIFO
 */
static int s3c_fifo_status(struct usb_ep *_ep)
{
	int count = 0;
	struct s3c_ep *ep;

	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep) {
		DEBUG_UDC("%s: bad ep\n", __func__);
		return -ENODEV;
	}

	DEBUG_UDC("%s: %d\n", __func__, ep_index(ep));

	/* LPD can't report unclaimed bytes from IN fifos */
	if (ep_is_in(ep))
		return -EOPNOTSUPP;

	return count;
}

/** Flush EP FIFO
 */
static void s3c_fifo_flush(struct usb_ep *_ep)
{
	struct s3c_ep *ep;

	ep = container_of(_ep, struct s3c_ep, ep);
	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		DEBUG_UDC("%s: bad ep\n", __func__);
		return;
	}

	DEBUG_UDC("%s: %d\n", __func__, ep_index(ep));
}

/* ---------------------------------------------------------------------------
 *	device-scoped parts of the api to the usb controller hardware
 * ---------------------------------------------------------------------------
 */

static int s3c_udc_get_frame(struct usb_gadget *_gadget)
{
	struct s3c_udc *udc = container_of(_gadget, struct s3c_udc, gadget);
	/*fram count number [21:8]*/
	unsigned int frame = __raw_readl(udc->regs + S3C_UDC_OTG_DSTS);

	DEBUG_UDC("%s: %p\n", __func__, _gadget);
	return frame & 0x3ff00;
}

static int s3c_udc_wakeup(struct usb_gadget *_gadget)
{
	DEBUG_UDC("%s: %p\n", __func__, _gadget);
	return -ENOTSUPP;
}

static int s3c_set_selfpowered(struct usb_gadget *_gadget, int is_on)
{
	struct s3c_udc *udc = container_of(_gadget, struct s3c_udc, gadget);
	unsigned long	flags;

	spin_lock_irqsave(&udc->lock, flags);
	udc->selfpowered = (is_on != 0);
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static void s3c_udc_soft_connect(void)
{
	struct s3c_udc *udc = the_controller;
	u32 uTemp;

	DEBUG_UDC("[%s]\n", __func__);
	uTemp = __raw_readl(udc->regs + S3C_UDC_OTG_DCTL);
	uTemp = uTemp & ~SOFT_DISCONNECT;
	__raw_writel(uTemp, udc->regs + S3C_UDC_OTG_DCTL);
}

static void s3c_udc_soft_disconnect(void)
{
	struct s3c_udc *udc = the_controller;
	u32 uTemp;
	unsigned long flags;

	DEBUG_UDC("[%s]\n", __func__);
	uTemp = __raw_readl(udc->regs + S3C_UDC_OTG_DCTL);
	uTemp |= SOFT_DISCONNECT;
	__raw_writel(uTemp, udc->regs + S3C_UDC_OTG_DCTL);

	spin_lock_irqsave(&udc->lock, flags);
	stop_activity(udc, udc->driver);
	spin_unlock_irqrestore(&udc->lock, flags);
}

static int s3c_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	if (is_on)
		s3c_udc_soft_connect();
	else
		s3c_udc_soft_disconnect();

	pullup_state = is_on;
	return 0;
}

static const struct usb_gadget_ops s3c_udc_ops = {
	.get_frame = s3c_udc_get_frame,
	.wakeup = s3c_udc_wakeup,
	.set_selfpowered = s3c_set_selfpowered,
	.pullup = s3c_udc_pullup,
	.vbus_session = s3c_vbus_enable,
	.udc_start = s3c_udc_start,
	.udc_stop = s3c_udc_stop,
};

static void nop_release(struct device *dev)
{
	DEBUG_UDC("%s %d\n", __func__, __LINE__);
}

static struct s3c_udc memory = {
	.selfpowered = true,
	.usb_address = 0,

	.gadget = {
		.ops = &s3c_udc_ops,
		.ep0 = &memory.ep[0].ep,
		.name = driver_name,
		.dev = {
			.release = nop_release,
		},
	},

	/* control endpoint */
	.ep[0] = {
		.ep = {
			.name = ep0name,
			.ops = &s3c_ep_ops,
			.maxpacket = EP0_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = 0,
		.bmAttributes = 0,

		.ep_type = ep_control,
		.fifo = (unsigned int) S3C_UDC_OTG_EP0_FIFO,
	},

	/* first group of endpoints */
	.ep[1] = {
		.ep = {
			.name = "ep1in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 1,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP1_FIFO,
	},

	.ep[2] = {
		.ep = {
			.name = "ep2out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 2,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP2_FIFO,
	},

	.ep[3] = {
		.ep = {
			.name = "ep3in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 3,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP3_FIFO,
	},
	.ep[4] = {
		.ep = {
			.name = "ep4out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 4,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP4_FIFO,
	},
	.ep[5] = {
		.ep = {
			.name = "ep5in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 5,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP5_FIFO,
	},
	.ep[6] = {
		.ep = {
			.name = "ep6in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 6,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP6_FIFO,
	},
	.ep[7] = {
		.ep = {
			.name = "ep7out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 7,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP7_FIFO,
	},
	.ep[8] = {
		.ep = {
			.name = "ep8in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 8,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP8_FIFO,
	},
	.ep[9] = {
		.ep = {
			.name = "ep9in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 9,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP9_FIFO,
	},
	.ep[10] = {
		.ep = {
			.name = "ep10out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 10,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP10_FIFO,
	},
	.ep[11] = {
		.ep = {
			.name = "ep11in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 11,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP11_FIFO,
	},
	.ep[12] = {
		.ep = {
			.name = "ep12in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 12,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP12_FIFO,
	},
	.ep[13] = {
		.ep = {
			.name = "ep13out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 13,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP13_FIFO,
	},
	.ep[14] = {
		.ep = {
			.name = "ep14in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 14,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP14_FIFO,
	},
	.ep[15] = {
		.ep = {
			.name = "ep15-iso",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE2,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 15,
		.bmAttributes = USB_ENDPOINT_XFER_ISOC,

		.ep_type = ep_isochronous,
		.fifo = (unsigned int) S3C_UDC_OTG_EP15_FIFO,
	},
};

/*
 *	probe - binds to the platform device
 */
static int s3c_udc_probe(struct platform_device *pdev)
{
	struct s3c_udc *udc = &memory;
	struct device *dev = &pdev->dev;
	struct usb_phy *phy;
	struct resource *res;
	unsigned int irq;
	int retval;

	DEBUG_UDC("%s: %p\n", __func__, pdev);

	if (!dev->platform_data && !pdev->dev.of_node) {
		dev_err(dev, "No platform data defined\n");
		return -EINVAL;
	}

	if (pdev->num_resources != 2) {
                dev_err(dev, "invalid num_resources\n");
                return -ENODEV;
        }
        if ((pdev->resource[0].flags != IORESOURCE_MEM)
                        || (pdev->resource[1].flags != IORESOURCE_IRQ)) {
                dev_err(dev, "invalid resource type\n");
                return -ENODEV;
        }

	phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
	if (IS_ERR(phy)) {
		dev_err(dev, "no platform data or transceiver defined\n");
		return -EPROBE_DEFER;
	} else {
		udc->phy = phy;
	}

	spin_lock_init(&udc->lock);
	udc->dev = pdev;

	udc->gadget.dev.parent = dev;
	udc->gadget.dev.dma_mask = pdev->dev.dma_mask;
	dev_set_name(&udc->gadget.dev, "gadget");

	udc->gadget.speed = USB_SPEED_UNKNOWN;
	udc->gadget.max_speed = USB_SPEED_HIGH;
	udc->gadget.is_otg = 0;
	udc->gadget.is_a_peripheral = 0;
	udc->gadget.b_hnp_enable = 0;
	udc->gadget.a_hnp_support = 0;
	udc->gadget.a_alt_hnp_support = 0;

	the_controller = udc;
	platform_set_drvdata(pdev, udc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		DEBUG_UDC(KERN_ERR "cannot find register resource 0\n");
		return -EINVAL;
	}

	udc->regs = devm_request_and_ioremap(dev, res);
	if (!udc->regs) {
		DEBUG_UDC(KERN_ERR "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}
	s3c_udc_reinit(udc);

	udc->clk = clk_get(dev, "otg");

	if (IS_ERR(udc->clk)) {
		dev_err(dev, "Failed to get clock\n");
		return -ENXIO;
	}

	udc->usb_ctrl = dma_alloc_coherent(&pdev->dev,
			sizeof(struct usb_ctrlrequest)*BACK2BACK_SIZE,
			&udc->usb_ctrl_dma, GFP_KERNEL);

	if (!udc->usb_ctrl) {
		DEBUG_UDC(KERN_ERR "%s: can't get usb_ctrl dma memory\n",
			driver_name);
		retval = -ENOMEM;
		goto err_get_ctrl_buf;
	}

	udc->ep0_data = dma_alloc_coherent(&pdev->dev,
			EP0_FIFO_SIZE,
			&udc->ep0_data_dma, GFP_KERNEL);

	if (!udc->ep0_data) {
		DEBUG_UDC(KERN_ERR "%s: can't get ep0_data dma memory\n",
			driver_name);
		retval = -ENOMEM;
		goto err_get_data_buf;
	}

	clk_prepare_enable(udc->clk);
	/* Mask any interrupt left unmasked by the bootloader */
	__raw_writel(0, udc->regs + S3C_UDC_OTG_GINTMSK);

	/* irq setup after old hardware state is cleaned up */
	irq = platform_get_irq(pdev, 0);
	retval =
	    request_irq(irq, s3c_udc_irq, 0, driver_name, udc);

	if (retval != 0) {
		DEBUG_UDC(KERN_ERR "%s: can't get irq %i, err %d\n", driver_name,
		      udc->irq, retval);
		retval = -EBUSY;
		goto err_irq;
	}
	udc->irq = irq;

	disable_irq(udc->irq);
	clk_disable_unprepare(udc->clk);

	retval = usb_add_gadget_udc(dev, &udc->gadget);
	if (retval) {
		dev_err(dev, "failed to add gadget to udc list\n");
		goto err_add_udc;
	}

	create_proc_files();

	return retval;

err_add_udc:
	device_unregister(&udc->gadget.dev);
	free_irq(udc->irq, udc);
err_irq:
	dma_free_coherent(&pdev->dev,
			EP0_FIFO_SIZE,
			udc->ep0_data, udc->ep0_data_dma);
err_get_data_buf:
	dma_free_coherent(&pdev->dev,
			sizeof(struct usb_ctrlrequest)*BACK2BACK_SIZE,
			udc->usb_ctrl, udc->usb_ctrl_dma);
err_get_ctrl_buf:
	clk_disable_unprepare(udc->clk);
	return retval;
}

static int s3c_udc_remove(struct platform_device *pdev)
{
	struct s3c_udc *udc = platform_get_drvdata(pdev);

	DEBUG_UDC("%s: %p\n", __func__, pdev);

	remove_proc_files();
	usb_gadget_unregister_driver(udc->driver);
	usb_del_gadget_udc(&udc->gadget);
	device_unregister(&udc->gadget.dev);

	clk_put(udc->clk);
	if (udc->ep0_data)
		dma_free_coherent(&pdev->dev,
				EP0_FIFO_SIZE,
				udc->ep0_data, udc->ep0_data_dma);
	if (udc->usb_ctrl)
		dma_free_coherent(&pdev->dev,
				sizeof(struct usb_ctrlrequest)*BACK2BACK_SIZE,
				udc->usb_ctrl, udc->usb_ctrl_dma);
	free_irq(udc->irq, udc);

	platform_set_drvdata(pdev, 0);

	the_controller = 0;

	return 0;
}

#ifdef CONFIG_PM
static int s3c_udc_suspend(struct device *dev)
{
	struct s3c_udc *udc = the_controller;
	int i;

	if (udc->driver) {
		if (udc->driver->suspend)
			udc->driver->suspend(&udc->gadget);

		/* Terminate any outstanding requests  */
		for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
			struct s3c_ep *ep = &udc->ep[i];
			unsigned long flags;

			spin_lock_irqsave(&udc->lock, flags);
			ep->stopped = 1;
			nuke(ep, -ESHUTDOWN);
			spin_unlock_irqrestore(&udc->lock, flags);
		}

		if (udc->driver->disconnect)
			udc->driver->disconnect(&udc->gadget);

		if (is_nonswitch())
			s3c_udc_disable(udc);
	}

	return 0;
}

static int s3c_udc_resume(struct device *dev)
{
	struct s3c_udc *udc = the_controller;

	if (udc->driver) {
		s3c_udc_reinit(udc);
		if (is_nonswitch())
			s3c_udc_enable(udc);
		s3c_udc_soft_connect();

		if (udc->driver->resume)
			udc->driver->resume(&udc->gadget);
	}

	return 0;
}
#else
#define s3c_udc_suspend NULL
#define s3c_udc_resume  NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops s3c_udc_pm_ops = {
	.suspend	= s3c_udc_suspend,
	.resume		= s3c_udc_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id s3c_udc_dt_match[] = {
	{
		.compatible = "samsung,exynos3-hsotg",
		.data = NULL,
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_usbphy_dt_match);
#endif

static struct platform_driver s3c_udc_driver = {
	.probe		= s3c_udc_probe,
	.remove		= s3c_udc_remove,
	.driver		= {
		.name	= "samsung,exynos3-hsotg",
		.owner	= THIS_MODULE,
		.pm	= &s3c_udc_pm_ops,
		.of_match_table	= of_match_ptr(s3c_udc_dt_match),
	},
};

module_platform_driver(s3c_udc_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Pankaj Dubey <pankaj.dubey@samsung.com");
MODULE_LICENSE("GPL");

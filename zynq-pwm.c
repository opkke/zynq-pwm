/*
 *Xilinx pwm driver for axi_timer IP.
 *
 *This program is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2
 *as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/io.h>

//  axi_timer regiser mapping 
#define OFFSET      0x10
#define DUTY        0x14
#define PERIOD      0x04
#define PWM_CONF    0x00000206
#define PWM_ENABLE  0x00000080

struct zynq_pwm_chip {
    struct device *dev;
    struct pwm_chip chip;
    int scaler;
    void __iomem *base_addr;
    struct clk *clk;
};

static inline struct zynq_pwm_chip *to_zynq_pwm_chip(struct pwm_chip *chip){
    return container_of(chip, struct zynq_pwm_chip, chip);
}

static int zynq_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns){
    struct zynq_pwm_chip *pc;
    pc = container_of(chip, struct zynq_pwm_chip, chip);
    iowrite32((duty_ns/pc->scaler) - 2, pc->base_addr + DUTY);
    iowrite32((period_ns/pc->scaler) - 2, pc->base_addr + PERIOD);
    return 0;
}

static int zynq_pwm_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm, enum pwm_polarity polarity){
    struct zynq_pwm_chip *pc;
    pc = to_zynq_pwm_chip(chip);
    return 0;
    /* no implementation of polarity function
       the axi timer hw block doesn't support this */
}

static int zynq_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm){
    struct zynq_pwm_chip *pc;
    pc = container_of(chip, struct zynq_pwm_chip, chip);
    iowrite32(ioread32(pc->base_addr) | PWM_ENABLE, pc->base_addr);
    iowrite32(ioread32(pc->base_addr + OFFSET) | PWM_ENABLE, pc->base_addr + OFFSET);
    return 0;
}

static void zynq_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm){
    struct zynq_pwm_chip *pc;
    pc = to_zynq_pwm_chip(chip);
    iowrite32(ioread32(pc->base_addr) & ~PWM_ENABLE, pc->base_addr);
    iowrite32(ioread32(pc->base_addr + OFFSET) & ~PWM_ENABLE, pc->base_addr + OFFSET);
}

static int zynq_pwm_remove(struct platform_device *pdev) {
    struct zynq_pwm_chip *zynq_pwm = platform_get_drvdata(pdev);
    clk_disable_unprepare(zynq_pwm->clk);
    return pwmchip_remove(&zynq_pwm->chip);
}

static const struct pwm_ops zynq_pwm_ops = {
    .config = zynq_pwm_config,
    .set_polarity = zynq_pwm_set_polarity,
    .enable = zynq_pwm_enable,
    .disable = zynq_pwm_disable,
    .owner = THIS_MODULE,
};

struct zynq_pwm_data {
    unsigned int duty_event;
};

static const struct of_device_id zynq_pwm_of_match[] = {
    { .compatible = "xlnx,xps-timer-1.00.a" },
    {}
};
MODULE_DEVICE_TABLE(of, zynq_pwm_of_match);

static int zynq_pwm_probe(struct platform_device *pdev) {
    struct zynq_pwm_chip *zynq_pwm;
    struct pwm_device *pwm;
    struct resource *res;
    u32 start, end;
    int ret,i;
    // allocate memory
    zynq_pwm = devm_kzalloc(&pdev->dev, sizeof(*zynq_pwm), GFP_KERNEL);
    if (!zynq_pwm)
        return -ENOMEM;    
    // get the base addr to res
    zynq_pwm->dev = &pdev->dev;
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) 
            return -ENODEV;
    zynq_pwm->base_addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(zynq_pwm->base_addr))
        return PTR_ERR(zynq_pwm->base_addr);
    // get the clock
    zynq_pwm->clk = devm_clk_get(&pdev->dev, NULL);
        if (IS_ERR(zynq_pwm->clk)) {
	    dev_err(&pdev->dev, "failed to get pwm clock\n");
	    return PTR_ERR(zynq_pwm->clk);
    }
    zynq_pwm->scaler = (int)1000000000/clk_get_rate(zynq_pwm->clk);
    start = res->start;
    end = res->end;
    // pwm_chip settings binding
    zynq_pwm->chip.dev = &pdev->dev;
    zynq_pwm->chip.ops = &zynq_pwm_ops;
    zynq_pwm->chip.base = -1;
    zynq_pwm->chip.npwm = 1;
    zynq_pwm->chip.of_xlate = &of_pwm_xlate_with_flags;
    zynq_pwm->chip.of_pwm_n_cells = 1;
    ret = pwmchip_add(&zynq_pwm->chip);
    if (ret < 0) {
        dev_err(&pdev->dev, "pwmchip_add failed: %d\n", ret);
        goto disable_pwmclk;
     }   
    // add each pwm device
    for (i = 0; i < zynq_pwm->chip.npwm; i++) {
            struct zynq_pwm_data *data;
            pwm = &zynq_pwm->chip.pwms[i];

            data = devm_kzalloc(zynq_pwm->dev, sizeof(*data), GFP_KERNEL);
            if (!data) {
                ret = -ENOMEM;
                goto pwmchip_remove;
            }
            pwm_set_chip_data(pwm, data);
    }
    
    iowrite32(PWM_CONF, zynq_pwm->base_addr);
    iowrite32(PWM_CONF, zynq_pwm->base_addr + OFFSET);
    // after finish all task below, set driver data to platform.
    platform_set_drvdata(pdev, pwm);
    return 0;
    pwmchip_remove:
    pwmchip_remove(&zynq_pwm->chip);
    disable_pwmclk:
    clk_disable_unprepare(zynq_pwm->clk);
    return ret;

}

static struct platform_driver zynq_pwm_driver = {
    .driver = {
        .name = "zynq-pwm",
        .of_match_table = zynq_pwm_of_match,
    },
    .probe = zynq_pwm_probe,
    .remove = zynq_pwm_remove,
};
module_platform_driver(zynq_pwm_driver);

MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("A Xilinx pwm driver");
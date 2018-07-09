#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/pwm.h>
#include <linux/device.h>

struct zynq_pwm_chip {
    // gpio has no device?
    struct device *dev;
    struct pwm_chip chip;
    void __iomem *base_addr;
    struct clk *clk;
};

static const struct pwm_ops zynq_pwm_ops = {
    .config = zynq_pwm_config,
    .set_polarity = zynq_pwm_set_polarity,
    .enable = zynq_pwm_enable,
    .disable = zynq_pwm_disable,
    .request = zynq_pwm_request,
    .free = zynq_pwm_free,
    .owner = THIS_MODULE,
};

struct zynq_pwm_data {
    unsigned int duty_event;
};


static const struct of_device_id lpc18xx_pwm_of_match[] = {
    { .compatible = "xlnx,xps-timer-1.00.a" },
    {}
};
MODULE_DEVICE_TABLE(of, lpc18xx_pwm_of_match);


static int zynq_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns);
static int zynq_pwm_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm, enum pwm_polarity polarity);
static int zynq_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm);
static void zynq_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm);
static int zynq_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm);
static void zynq_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm);
static int zynq_pwm_remove(struct platform_device *pdev);


static int zynq_pwm_probe(struct platform_device *pdev) {
    struct zynq_pwm_chip *zynq_pwm;
    struct resource *res;
    struct pwm_device *pwm;
    // allocate memory
    pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL)
    if (!pwm)
        return -ENOMEM;
    // get the base addr to res
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) 
            return -ENODEV;
    zynq_pwm->base_addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(zynq_pwm->base_addr))
        return PTR_ERR(zynq_pwm->base_addr);
    // get the clock
    zynq_pwm->clk = devm_clk_get($pdev->dev, NULL);

    // pwm_chip settings binding
    zynq_pwm->chip.dev = &pdev->dev;
    zynq_pwm->chip.ops = &zynq_pwm_ops;
    zynq_pwm->chip.base = -1;
    zynq_pwm->chip.npwm = 1;
    zynq_pwm->chip.of_xlate = of_pwm_xlate_with_flags;
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
                goto remove_pwmchip;
            }

            pwm_set_chip_data(pwm, data);
    }

    // after finish all task below, set driver data to platform.
    platform_set_drvdata(pdev, pwm)

    remove_pwmchip:
    pwmchip_remove(&zynq_pwm->chip);
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
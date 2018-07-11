#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/pwm.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>

struct zynq_pwm_chip {
    // gpio has no device?
    struct device *dev;
    struct pwm_chip chip;
    void __iomem *base_addr;
    struct clk *clk;
};

static int zynq_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns);
static int zynq_pwm_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm, enum pwm_polarity polarity);
static int zynq_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm);
static void zynq_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm);
static int zynq_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm);
static void zynq_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm);
static int zynq_pwm_remove(struct platform_device *pdev);

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


static const struct of_device_id zynq_pwm_of_match[] = {
    { .compatible = "xlnx,xps-timer-1.00.a" },
    {}
};
MODULE_DEVICE_TABLE(of, zynq_pwm_of_match);





static int zynq_pwm_probe(struct platform_device *pdev) {
    struct zynq_pwm_chip *zynq_pwm;
    struct pwm_device *pwm;
    struct resource *res;
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
    // pwm_chip settings binding
    zynq_pwm->chip.dev = &pdev->dev;
    zynq_pwm->chip.ops = &zynq_pwm_ops;
    zynq_pwm->chip.base = -1;
    zynq_pwm->chip.npwm = 2;
    //bug??
    //zynq_pwm->chip.of_xlate = of_pwm_xlate_with_flags;
    zynq_pwm->chip.of_pwm_n_cells = 3;

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

    // after finish all task below, set driver data to platform.
    platform_set_drvdata(pdev, pwm);
    return 0;

    pwmchip_remove:
    pwmchip_remove(&zynq_pwm->chip);
    disable_pwmclk:
    clk_disable_unprepare(zynq_pwm->clk);
    return ret;

}

static int zynq_pwm_remove(struct platform_device *pdev) {
    struct zynq_pwm_chip *zynq_pwm = platform_get_drvdata(pdev);
    clk_disable_unprepare(zynq_pwm->clk);
    return pwmchip_remove(&zynq_pwm->chip);
}

static struct platform_driver zynq_pwm_driver = {
    .driver = {
        .name = "zynq-pwm",
        .of_match_table = zynq_pwm_of_match,
    },
    .probe = zynq_pwm_probe,
    .remove = zynq_pwm_remove,
};

static int __init zynq_pwm_init(void)
{
	return platform_driver_register(&zynq_pwm_driver);
}
subsys_initcall(zynq_pwm_init);

static void __exit zynq_pwm_exit(void)
{
	platform_driver_unregister(&zynq_pwm_driver);
}
module_exit(zynq_pwm_exit);

MODULE_LICENSE("GPL"); 

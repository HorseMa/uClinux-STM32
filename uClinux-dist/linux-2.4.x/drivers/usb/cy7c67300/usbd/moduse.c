
{

    if (driver->module != NULL)
        __MOD_INC_USE_COUNT(driver->module);

    down(&driver->serialize);
    driver->disconnect(dev,priv);
    up(&driver->serialize);

    if (driver->module != NULL)
        __MOD_DEC_USE_COUNT(driver->module);

}



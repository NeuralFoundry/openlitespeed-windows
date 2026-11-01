<?php

namespace LSWebAdmin\Product\Ows;

use LSWebAdmin\Controller\ControllerBase;

class Service extends ControllerBase
{
    protected function getConfigDataClass()
    {
        return ConfigData::class;
    }

    protected function getPidFile()
    {
        return (isset($_SERVER['LS_TMP_DIR']) ? $_SERVER['LS_TMP_DIR'] : '/tmp') . '/lshttpd/lshttpd.pid';
    }

    protected function getStatusFile()
    {
        return (isset($_SERVER['LS_TMP_DIR']) ? $_SERVER['LS_TMP_DIR'] : '/tmp') . '/lshttpd/.status';
    }
}

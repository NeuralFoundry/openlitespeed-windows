<?php

namespace LSWebAdmin\Product\WebServer\Ols;

use LSWebAdmin\Product\Base\RealTimeStatsBase as ProductRealTimeStatsBase;

class RealTimeStatsBase extends ProductRealTimeStatsBase
{
    protected function getStatsDir()
    {
        return (isset($_SERVER['LS_TMP_DIR']) ? $_SERVER['LS_TMP_DIR'] : '/tmp') . '/lshttpd';
    }

    protected function getReportProcessCount()
    {
        return isset($_SERVER['LSWS_CHILDREN']) ? (int) $_SERVER['LSWS_CHILDREN'] : 1;
    }
}

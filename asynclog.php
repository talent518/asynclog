<?php
extension_loaded('asynclog') or die('The asynclog extension is not loaded');

echo 'config: ', json_encode(ini_get_all('asynclog'), JSON_PRETTY_PRINT), PHP_EOL;
echo 'constants: ', json_encode(get_defined_constants(true)['asynclog'], JSON_PRETTY_PRINT), PHP_EOL;
echo asynclog('app', ASYNCLOG_LEVEL_INFO, 'test', $_SERVER), PHP_EOL;

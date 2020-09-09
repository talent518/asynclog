<?php
extension_loaded('asynclog') or die('The asynclog extension is not loaded');

echo 'config: ', json_encode(ini_get_all('asynclog'), JSON_PRETTY_PRINT), PHP_EOL;
echo 'constants: ', json_encode(get_defined_constants(true)['asynclog'], JSON_PRETTY_PRINT), PHP_EOL;
var_export(asynclog('app', 'test', ASYNCLOG_LEVEL_INFO, 'Message Content', $_SERVER));

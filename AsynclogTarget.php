<?php
namespace app\core;

use app\utils\StringHelper;
use yii\base\Application;
use yii\base\ErrorHandler;
use yii\log\Logger;
use yii\log\Target;

class AsynclogTarget extends Target {
	private $stack = [];

	public function formatMessage($msg) {
		@list($message, $level, $category, $timestamp, $traces, $memory) = $msg;
		
		$data = compact('timestamp', 'memory');

		switch($level) {
			case Logger::LEVEL_ERROR:
				$level = ASYNCLOG_LEVEL_ERROR;
				break;
			case Logger::LEVEL_WARNING:
				$level = ASYNCLOG_LEVEL_WARN;
				break;
			case Logger::LEVEL_INFO:
				$level = ASYNCLOG_LEVEL_INFO;
				break;
			case Logger::LEVEL_TRACE:
				$level = ASYNCLOG_LEVEL_DEBUG;
				break;
			case Logger::LEVEL_PROFILE_BEGIN:
				$level = ASYNCLOG_LEVEL_VERBOSE;
				$hash = md5(json_encode($token));
				$this->stack[$hash] = $msg;
				return;
			case Logger::LEVEL_PROFILE_END:
				$level = ASYNCLOG_LEVEL_VERBOSE;
				if (isset($this->stack[$hash])) {
					@list($message, , $category, $timestamp2, $traces, $memory2) = $this->stack[$hash];
					unset($this->stack[$hash]);
					$data['timestamp'] = $timestamp2;
					$data['duration'] = $timestamp - $timestamp2;
					$data['memoryDiff'] = $memory - $memory2;
					ksort($data);
					break;
				} else {
					return;
				}
			default:
				return;
		}

		if(!is_string($message)) {
			if($message instanceof \Throwable ) {
				$message = ErrorHandler::convertExceptionToString($message);
			} else {
				$message = json_encode($message, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
			}
		}

		$data['user'] = [];

		foreach(['AUTH_UID', 'AUTH_PWD', 'AUTH_FID', 'USER_NICKNAME', 'USER_USERNAME', 'USER_ORG_ID', 'USER_IS_MANAGER', 'USER_IDENTITY_ID', 'USER_MOBILE', 'USER_IDENTITY_NAME', 'USER_IDENTITYS'] as $name) {
			if(defined($name)) {
				$data['user'][$name] = constant($name);
			}
		}

		foreach($traces as $trace) {
			$data['traces'][] = sprintf('%s%s%s in %s:%d', $trace['class'], $trace['type'], $trace['function'], $trace['file'], $trace['line']);
		}

		asynclog('sing-agent', $category, $level, $message, $data, $data['timestamp'], $data['duration']??-1);
	}
	
	public function export() {
		foreach($this->messages as $message) {
			$this->formatMessage($message);
		}
	}
}
<?php
/**
 * @file
 * media_theplatform_mpx/includes/media_theplatform_mpx.variables.inc
 *
 * Define Media:mpx variables.
 */

/**
 * This is the variable namespace, automatically prepended to module variables.
 */
define('media_theplatform_mpx_NAMESPACE', 'media_theplatform_mpx__');

// Use this string to extract a Player's id when importing.
define('media_theplatform_mpx_ID_PREFIX', 'http://data.player.theplatform.com/player/data/Player/');

/**
 * Wrapper for variable_get() using the Media: mpx variable registry.
 *
 * @see media_theplatform_mpx_variable_set()
 * @see media_theplatform_mpx_variable_del()
 * @see media_theplatform_mpx_variable_default()
 */
function media_theplatform_mpx_variable_get($name, $default = NULL) {
  if (!isset($default)) {
    $default = media_theplatform_mpx_variable_default($name);
  }
  // Namespace all variables.
  $variable_name = media_theplatform_mpx_NAMESPACE . $name;
  return variable_get($variable_name, $default);
}

/**
 * Wrapper for variable_set() using the Media: mpx variable registry.
 *
 * @see media_theplatform_mpx_variable_get()
 * @see media_theplatform_mpx_variable_del()
 * @see media_theplatform_mpx_variable_default()
 */
function media_theplatform_mpx_variable_set($name, $value) {
  $variable_name = media_theplatform_mpx_NAMESPACE . $name;
  return variable_set($variable_name, $value);
}

/**
 * Wrapper for variable_del() using the Media: mpx variable registry.
 *
 * @see media_theplatform_mpx_variable_get()
 * @see media_theplatform_mpx_variable_set()
 * @see media_theplatform_mpx_variable_default()
 */
function media_theplatform_mpx_variable_del($name) {
  $variable_name = media_theplatform_mpx_NAMESPACE . $name;
  variable_del($variable_name);
}

/**
 * The default variables within the Media: ThePlatform namespace.
 *
 * @see media_theplatform_mpx_variable_get()
 * @see media_theplatform_mpx_variable_set()
 * @see media_theplatform_mpx_variable_del()
 */
function media_theplatform_mpx_variable_default($name = NULL) {
  static $defaults;

  if (!isset($defaults)) {
    $defaults = array(
      'account_pid' => NULL,
      'cron_players' => 1,
      'cron_videos' => 1,
      'default_player_fid' => NULL,
      'feed' => NULL,
      'import_account' => NULL,
      'password' => NULL,
      'token' => NULL,
      'username' => NULL,
    );
  }

  if (!isset($name)) {
    return $defaults;
  }

  if (isset($defaults[$name])) {
    return $defaults[$name];
  }
}

/**
 * Return the fully namespace variable name.
 *
 * @param string $name
 *   The variable name to retrieve the namespaced name.
 *
 * @return string
 *   The fully namespace variable name, prepended with
 *   media_theplatform_mpx_NAMESPACE.
 */
function media_theplatform_mpx_variable_name($name) {
  return media_theplatform_mpx_NAMESPACE . $name;
}

<?php

/**
 * @file
 * Helper functions
 */

/**
 * Signs into thePlatform and sets/returns signIn token variable.  Returns FALSE if fails.
 */
function media_theplatform_mpx_signin() {
  $username = media_theplatform_mpx_variable_get('username');
  $pass = media_theplatform_mpx_variable_get('password');
  // IdleTimeout is stored in milliseconds.  Set to 2 weeks.
  $idle_timeout = 1209600000;
  $login_url = "https://identity.auth.theplatform.com/idm/web/Authentication/signIn?schema=1.0&form=json&_idleTimeout=" . $idle_timeout;
  $data = 'username=' . $username . '&password=' . $pass;
  $options = array(
    'method' => 'POST',
    'data' => $data,
    'timeout' => 15,
    'headers' => array('Content-Type' => 'application/x-www-form-urlencoded'),
  );
  $result = drupal_http_request($login_url, $options);
  $result_data = drupal_json_decode($result->data);
  // If valid response, set token variable.
  if (isset($result_data['signInResponse']['token'])) {
    $mpx_token = $result_data['signInResponse']['token'];
    media_theplatform_mpx_variable_set('token', $mpx_token);
    // Store unix timestamp of when this token will idleTimeout.
    media_theplatform_mpx_variable_set('date_idletimeout', time() + $idle_timeout / 1000);
    $action = 'signIn successful';
  }
  // Else DESTROY the token variable!
  else {
    $mpx_token = FALSE;
    media_theplatform_mpx_variable_set('token', NULL);
    $action = 'signIn failed';
  }
  // Write log record to mpx_log.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'settings',
    'type_id' => NULL,
    'action' => $action,
    'details' => NULL,
  );
  media_theplatform_mpx_insert_log($log);
  return $mpx_token;
}

/**
 * Returns array of all accounts specified thePlatform account.
 */
function media_theplatform_mpx_get_accounts_select() {
  // Check for the signIn token.
  $mpx_token = media_theplatform_mpx_variable_get('token', NULL);
  if (!$mpx_token) {
    return t('There was an error with your request.');
  }
  // Get the list of accounts from thePlatform.
  $url = 'http://access.auth.theplatform.com/data/Account?schema=1.3.0&form=json&byDisabled=false&token=' . $mpx_token;
  $result = drupal_http_request($url);
  $result_data = drupal_json_decode($result->data);

  global $user;

  if (empty($result_data['entryCount']) || $result_data['entryCount'] == 0) {
    $log = array(
      'uid' => $user->uid,
      'type' => 'request',
      'type_id' => NULL,
      'action' => 'account',
      'details' => '0 accounts returned.',
    );
    media_theplatform_mpx_insert_log($log);
    //return FALSE;
    drupal_set_message(t('The logged in user does not have the privilege to set the account.'), 'warning');
    return array();
  }
  $accounts = array();
  $accounts_data = array();

  foreach ($result_data['entries'] as $entry) {
    $title = $entry['title'];
    $key = rawurlencode($title);
    $accounts[$key] = $title;
  }
  $log = array(
    'uid' => $user->uid,
    'type' => 'request',
    'type_id' => NULL,
    'action' => 'account',
    'details' => count($accounts) . ' accounts returned.',
  );
  media_theplatform_mpx_insert_log($log);
  // Sort accounts alphabetically.
  natcasesort($accounts);
  return $accounts;
}

/**
 * Requests signin token if the current token's idleTimeout date has passed.
 */
function media_theplatform_mpx_check_token() {
  if (!media_theplatform_mpx_variable_get('token')) {
    return FALSE;
  }
  // If idleTimeout date has passed, signIn again.
  if (media_theplatform_mpx_variable_get('date_idletimeout') < time()) {
    // Expire the current token.
    media_theplatform_mpx_expire_token();
    // Retrieve and return new token.
    return media_theplatform_mpx_signin();
  }
  else {
    return TRUE;
  }
}

/**
 * Requests expiration of current signin token.
 */
function media_theplatform_mpx_expire_token() {
  $url = 'https://identity.auth.theplatform.com/idm/web/Authentication/signOut?schema=1.0&form=json&_token=' . media_theplatform_mpx_variable_get('token');
  $result = drupal_http_request($url); 
}

/**
 * Checks if file is active in its mpx datatable.
 *
 * @param Object $file
 *   A File Object.
 *
 * @return Boolean
 *   TRUE if the file is active, and FALSE if it isn't.
 */
function media_theplatform_mpx_is_file_active($file) {
  $wrapper = file_stream_wrapper_get_instance_by_uri($file->uri);
  $parts = $wrapper->get_parameters();
  if ($parts['mpx_type'] == 'player') {
    $player = media_theplatform_mpx_get_mpx_player_by_fid($file->fid);
    return $player['status'];
  }
  elseif ($parts['mpx_type'] == 'video') {
    $video = media_theplatform_mpx_get_mpx_video_by_field('guid', $parts['mpx_id']);
    return $video['status'];
  }
}

/************************ parsing strings *********************************/

/**
 * Return array of strings of all id's in thePlayer HTML/CSS.
 */
function media_theplatform_mpx_get_tp_ids() {
  return array(
    'categories',
    'header',
    'info',
    'player',
    'releases',
    'search',
    'tpReleaseModel1',
  );
}

/**
 * Finds all #id's in the HTML and appends with $new_id to each #id.
 *
 * @param String $html
 *   String of HTML that needs HTML id's altered.
 * @param String $new_id
 *   The string pattern we need to append to each id in $html.
 *
 * @return String
 *   $html with all id's as #mpx.$new_id, with tp:scopes variables.
 */
function media_theplatform_mpx_replace_html_ids($html, $new_id) {
  // Append new_id to all div id's.
  foreach (media_theplatform_mpx_get_tp_ids() as $tp_id) {
    $html = media_theplatform_mpx_append_html_id($tp_id, $new_id, $html);
  }
  return $html;
}

/**
 * Finds given $div_id in HTML, appends its id with $append.
 *
 * Also adds tp:scopes variable for tpPdk.js.
 *
 * @param String $div_id
 *   The div id we need to append a string to.
 * @param String $append
 *   The string we need to append.
 * @param String $html
 *   The string we need to search to find $find.
 *
 * @return String
 *   $html with $find replaced by $find.$append.
 */
function media_theplatform_mpx_append_html_id($div_id, $append, $html) {
  $find = 'id="' . $div_id . '"';
  // Replace with 'div_id-append'.
  $replace = 'id="' . $div_id . '-' . $append . '" tp:scopes="scope-' . $append . '"';
  return str_replace($find, $replace, $html);
}

/**
 * Finds all #id's in the CSS and appends with $new_id to each #id.
 *
 * @param String $html
 *   String of HTML that needs css id's altered.
 * @param String $new_id
 *   The string pattern we need to append to each #id in $html.
 *
 * @return String
 *   $html with all id's prepended with #mpx-$new_id selector
 */
function media_theplatform_mpx_replace_css_ids($html, $new_id) {

  // Append $new_id to each id selector.
  foreach (media_theplatform_mpx_get_tp_ids() as $tp_id) {
    $html = media_theplatform_mpx_append_string('#' . $tp_id, '-' . $new_id, $html);
  }
  // Replace body selector with #mpx_new_id
  $mpx_id = '#mpx-' . $new_id;
  $html = str_replace('body', $mpx_id, $html);
  // Get rid of tabs, newlines to make it easier to find all classes.
  $html = str_replace(array("\r\n", "\r", "\n", "\t"), '', $html);
  // Add #mpx_id as parent selector of all classes.
  $html = str_replace("}", "}\n " . $mpx_id . " ", $html);
  // Clean up the last }.
  $remove = strlen($mpx_id) + 1;
  $html = substr($html, 0, -$remove);
  // If any commas in the selectors, add mpx to each item after the comma.
  $html = str_replace(",", ", " . $mpx_id . " ", $html);
  return $html;
}

/**
 * Appends a pattern to another string pattern for given $html.
 *
 * @param String $find
 *   The string pattern we need to append a string to.
 * @param String $append
 *   The string we need to append.
 * @param String $html
 *   The string we need to search to find $find.
 *
 * @return String
 *   $html with $find replaced by $find.$append
 */
function media_theplatform_mpx_append_string($find, $append, $html) {
  $replace = $find . $append;
  return str_replace($find, $replace, $html);
}

/**
 * Alters mpxPlayer HTML to render a mpx_video by its Guid.
 *
 * Adds 'byGuid=$guid' to the tp:feedsserviceurl in div#tpReleaseModel.
 *
 * @param String $guid
 *   The Guid string of the mpx_video we want to render.
 * @param String $html
 *   String of mpxPlayer HTML to be used to render the mpx_video.
 *
 * @return String
 *   mpxPlayer HTML for the mpx_video.
 */
function media_theplatform_mpx_add_guid_to_html($guid, $html) {
  $tag = 'tp:feedsServiceURL="';
  // Get the current value for this tag.
  $default_url_value = media_theplatform_mpx_extract_string($tag, '"', $html);
  // Append the byGuid parameter to the current value.
  $new_url_value = $default_url_value . '?byGuid=' . $guid;
  $str_old = $tag . $default_url_value . '"';
  $str_new = $tag . $new_url_value . '"';
  return str_replace($str_old, $str_new, $html);
}

/**
 * Returns the string between two given strings.
 *
 * @param String $start_str
 *   The string pattern that begins what we want to extract.
 * @param String $end_str
 *   The string pattern that ends what we want to extract.
 * @param String $input
 *   The string we need to search.
 *
 * @return String
 *   The string between $start_str and $end_str.
 */
function media_theplatform_mpx_extract_string($start_str, $end_str, $input) {
  $pos_start = strpos($input, $start_str) + strlen($start_str);
  $pos_end = strpos($input, $end_str, $pos_start);
  $result = substr($input, $pos_start, $pos_end - $pos_start);
  return $result;
}

/**
 * Returns the File ID's from given Media WYSIWYG markup.
 *
 * @param String $text
 *   String of WYSIWYG markup.
 *
 * @return Array
 *   The File fid's that the markup contains.
 */
function media_theplatform_mpx_extract_fids($text) {
  $pattern = '/\"fid\":\"(.*?)\"/';
  preg_match_all($pattern, $text, $results);
  return $results[1];
}

/**
 * Returns array of URLs of any external CSS files referenced in $text.
 */
function media_theplatform_mpx_extract_all_css_links($text) {
  $pattern = '/\<link rel\=\"stylesheet\" type\=\"text\/css\" media\=\"screen\" href\=\"(.*?)\" \/\>/';
  preg_match_all($pattern, $text, $results);
  return $results[1];
}

/**
 * Returns array of css data for any <style> tags in $text.
 */
function media_theplatform_mpx_extract_all_css_inline($text) {
  $pattern = '/<style.*>(.*)<\/style>/sU';
  preg_match_all($pattern, $text, $results);
  return $results[1];
}

/**
 * Return array of URLs of any external JS files referenced in $text.
 */
function media_theplatform_mpx_extract_all_js_links($text) {
  $pattern = '/\<script type\=\"text\/javascript\" src\=\"(.*?)\"/';
  preg_match_all($pattern, $text, $results);
  $js_files = $results[1];
  return $js_files;
}

/**
 * Returns array of any inline JS data for all <script> tags in $text.
 */
function media_theplatform_mpx_extract_all_js_inline($text) {
  $pattern = '/<script type\=\"text\/javascript\">(.*)<\/script>/sU';
  preg_match_all($pattern, $text, $results);
  return $results[1];
}


/**
 * Returns string of CSS by requesting data from given stylesheet $href.
 */
function media_theplatform_mpx_get_external_css($href) {

  $result = drupal_http_request($href);
  // Grab its CSS.
  $css = $result->data;

  // If this is PDK stylesheet, change relative image paths to absolute.
  $parts = explode('/', $href);
  if ($parts[2] == 'pdk.theplatform.com') {
    // Remove filename.
    array_pop($parts);
    // Store filepath.
    $css_path = implode('/', $parts) . '/';
    // Replace all relative images with absolute path to skin_url.
    $css = str_replace("url('", "url('" . $css_path, $css);
  }
  return $css;
}

/**
 * Checks for a valid JSON response from an MPX API call
 * @param $response
 * @param $service
 * @return array
 */
function media_theplatform_mpx_check_json_response($response, $service) {
  // No response
  if(!strlen($response))
    return array('status' => 'error', 'response' => t('No response from @service', array('@service' => $service)));
  // Decode JSON
  $responseObject = json_decode($response);
  // Make sure the response decodes, if not, return it's text
  if(!is_object($responseObject))
    return array('status' => 'error', 'response' => t('Error response from @service: @response', array('@service' => $service, '@response' => $response)));
  // Check for an exception on the response, return it's description if set
  if(property_exists($responseObject, 'isException'))
    return array('status' => 'error', 'response' => t('Exception from @service: @response', array('@service' => $service, '@response' => $responseObject->description)));
  // Looking good, return the response object
  else
    return array('status' => 'success', 'response' => $responseObject);
}

/**
 * Checks for an empty response from an MPX API call
 * (as opposed to an error response)
 * @param $response
 * @param $service
 * @return array
 */
function media_theplatform_mpx_check_empty_response($response, $service) {
  // No response (this is what we want in this case)
  if(!strlen($response))
    return array('status' => 'success');

  // If there is a response, look for an exception
  $responseObject = json_decode($response);
  // Make sure the response decodes, if not, return it's text
  if(!is_object($responseObject))
    return array('status' => 'error', 'response' => t('Error response from @service: @response', array('@service' => $service, '@response' => $response)));
  // Check for an exception on the response, return it's description if set
  if(property_exists($responseObject, 'isException'))
    return array('status' => 'error', 'response' => t('Exception from @service: @response', array('@service' => $service, '@response' => $responseObject->description)));
}

/**
 * Check for a successful response code from a cURL request
 * @param $url
 * @param $responseCode
 * @return array
 */
function media_theplatform_mpx_check_curl_response_code($url, $responseCode) {
  if(strpos($responseCode, '2') === 0) {
    return array('status' => 'success');
  }
  else {
    return array('status' => 'error', 'response' => t('MPX cURL request to: @url failed with response code: @responseCode', array('@url' => $url, '@responseCode' => $responseCode)));
  }
}
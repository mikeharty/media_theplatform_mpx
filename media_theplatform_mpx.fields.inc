<?php
/**
 * @file
 * Helper functions around field syncing
 */

/**
 * Returns a list of custom fields in MPX
 * @return array|bool
 */
function _mpx_fields_get_mpx_fields() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = MPX_MD_BASE.'/media/data/Media/Field?&sort=title&token='.$mpx_token.'&account='.$account_id.'&schema=1.6.0&fields=title%2Cplfield%24dataType%2Cplfield%24dataStructure%2Cplfield%24allowedValues%2Cplfield%24length%2Cplfield%24fieldName&form=json';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    // Check for valid cURL response
    $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      return false;
    }

    // Check for JSON object response
    $response = media_theplatform_mpx_check_json_response($response, 'getPublishingProfiles');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      return false;
    }

    $fields = array();
    foreach($response['response']->entries as $field) {
      $fields[$field->{'plfield$fieldName'}]->title = $field->title;
      $fields[$field->{'plfield$fieldName'}]->dataType = $field->{'plfield$dataType'};
      $fields[$field->{'plfield$fieldName'}]->dataStructure = $field->{'plfield$dataStructure'};
    }

    return $fields;

  } else {
    drupal_set_message(t('Unable to sign in to MPX.'), 'error');
    return false;
  }
}

/**
 * Given an MPX Media Object and a map between Drupal and MPX fields,
 * this returns an array keyed by the Drupal field name with a value
 * containing the field value from MPX.
 * @param $data
 * @param $file_field_map
 * @return array|bool
 */
function _mpx_fields_extract_mpx_field_values($data, $file_field_map) {
  if(media_theplatform_mpx_check_token()) {
    $fields = array();
    $account_id = media_theplatform_mpx_variable_get('account_id');
    if(isset($data['$xmlns'])) {
      foreach($data['$xmlns'] as $ns => $val){
        if($val == $account_id)
          $custom_field_namespace = $ns;
      }

      if(isset($custom_field_namespace)) {
        foreach($file_field_map as $drupal_field => $mpx_field) {
          if(isset($data[$custom_field_namespace.'$'.$mpx_field])) {
            $fields[$drupal_field] = $data[$custom_field_namespace.'$'.$mpx_field];
          } else {
            $fi = field_info_field($drupal_field);
            if($fi['type'] == 'taxonomy_term_reference' && is_array($data['media$categories'])) {
              $terms = array();
              foreach($data['media$categories'] as $category) {
                $tid = db_select('mpx_categories', 'c')
                  ->fields('c', array('tid'))
                  ->condition('full_title', $category['media$name'], '=')
                  ->execute()
                  ->fetchField();
                if($tid)
                  $terms[] = $tid;
              }
              if(count($terms))
                $fields[$drupal_field] = $terms;
            }
          }
        }
      }
    }
    return $fields;
  } else {
    drupal_set_message(t('Unable to sign in to MPX.'), 'error');
    return false;
  }
}

/**
 * Adds mapped Drupal file fields to media object for insertion to MPX.
 * @param $media_object
 * @param $fields
 * @param bool $update (optional, changes the structure of the returned media object)
 * @return mixed
 */
function _mpx_build_media_object_fields($media_object, $fields, $update = false) {
  // Special handling for categories
  // @todo: This will need to be modified if multiple taxonomies are supported
  $file_field_map = media_theplatform_mpx_variable_get('file_field_map', false);
  if($file_field_map) {
    $file_field_map = unserialize($file_field_map);
    foreach($fields as $id => $val) {
      $fi = field_info_field($id);

      if($fi['type'] == 'taxonomy_term_reference') {
        if(!is_array($val))
          $val = array($val);
        $categories = array();
        foreach($val as $tid) {
          $full_title = db_select('mpx_categories', 'c')
            ->fields('c', array('full_title'))
            ->condition('tid', $tid, '=')
            ->execute()
            ->fetchField();
          if($full_title) {
            $category = new stdClass;
            $category->{'media$name'} = $full_title;
            $categories[] = $category;
          }
        }
        if(count($categories))
          if(!$update)
            $media_object->entries[0]->{'media$categories'} = $categories;
          else
            $media_object->{'media$categories'} = $categories;
      }
      else {
        if(strlen($val)) {
          $mpx_field_id = null;
          foreach($file_field_map as $drupal_field => $mpx_field) {
            if($id == $drupal_field) {
              $mpx_field_id = $mpx_field;
              break;
            }
          }
          if(!$update)
            $media_object->entries[0]->{"pl1$".$mpx_field_id} = $val;
          else
            $media_object->{"pl1$".$mpx_field_id} = $val;
        }
      }
    }
  }
  return $media_object;
}

/**
 * Updates a MediaObject in MPX with provided field values
 * @param $id - The ID of the media object to be updated
 * @param $fields - An array containing Drupal field names and values
 * @return bool
 */
function media_theplatform_mpx_update_fields_in_mpx($id, $fields) {
  if(media_theplatform_mpx_check_token()) {
    // Before we can sync, we need to make sure the synced taxonomy is up to date
    media_theplatform_mpx_taxonomy_sync();

    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $media_object = new stdClass;
    $media_object->{'$xmlns'}->media = 'http://search.yahoo.com/mrss/';
    $media_object->{'$xmlns'}->pl = 'http://xml.theplatform.com/data/object';
    $media_object->{'$xmlns'}->plmedia = 'http://xml.theplatform.com/media/data/Media';
    $media_object->{'$xmlns'}->pl1 = $account_id;
    $media_object->id = "http://data.media.theplatform.com/media/data/Media/".$id;
    $media_object = _mpx_build_media_object_fields($media_object, $fields, true);
    $media_object = json_encode($media_object);

    $url = MPX_MD_BASE.'/media/data/Media?schema=1.2&form=json&account='.$account_id.'&token='.$mpx_token.'&method=put';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_POST, 1);
    curl_setopt($ch, CURLOPT_POSTFIELDS, $media_object);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: text/plain'));
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      return false;
    }
    $response = media_theplatform_mpx_check_empty_response($response, 'UpdateMediaObject');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      return false;
    }
  } else {
    drupal_set_message(t('Unable to sign in to MPX.'), 'error');
    return false;
  }
}

/**
 * This provides rudimentary field data type mapping.
 *
 * Current limitations:
 *  Only supports single value fields (no lists, maps),
 *  Provides no casting ability or other format conversions
 *  Does not check field length limitations
 *
 * @param $drupal_field_id
 * @param $mpx_field_object
 * @return bool
 */
function _mpx_fields_check_compatibility($drupal_field_id, $mpx_field_object) {
  // Creating this as an array, eventually we may support some forms of casting
  $field_compatibility_map = array(
    'list_boolean' => array('Boolean'),
    'number_decimal' => array('Decimal'),
    'number_integer' => array('Integer'),
    'text' => array('String'),
    'text_long' => array('String'),
    'text_with_summary' => array('String'),
    'taxonomy_term_reference' => array('String'),
  );

  $drupal_field = field_info_field($drupal_field_id);
  $mpx_field_type = $mpx_field_object->dataType;
  $mpx_field_structure = $mpx_field_object->dataStructure;

  if(!isset($field_compatibility_map[$drupal_field['type']]) || !in_array($mpx_field_type, $field_compatibility_map[$drupal_field['type']]))
    return false;

  // We're only allowing single value fields on both sides (Excluding taxonomy fields)
  if($drupal_field['cardinality'] != 1 || $mpx_field_structure != 'Single')
    return false;

  return true;
}

/**
 * Given an array of submitted form fields, returns an array containing
 * the configured mapped fields with their values.
 * @param $fields
 * @return array
 */
function media_theplatform_mpx_process_file_form_fields($fields) {
  if(is_object($fields))
    $fields = (array) $fields;

  $processed = array();
  if($sync_fields = media_theplatform_mpx_variable_get('file_field_map', false)) {
    foreach(unserialize($sync_fields) as $field => $value) {
      $fi = field_info_field($field);
      switch($fi['type']) {
        // Taxonomy reference fields have a special key
        case 'taxonomy_term_reference':
          $key = 'tid';
          break;
        // @todo: check for other field type keys
        default:
          $key = 'value';
      }
      if(isset($fields[$field]['und'])) {
        // Parse multiple value fields
        if(count($fields[$field]['und']) > 1) {
          foreach($fields[$field]['und'] as $value) {
            $processed[$field][] = $value[$key];
          }
          // single value fields
        } else {
          if(isset($fields[$field]['und'][0]))
            $processed[$field] = $fields[$field]['und'][0][$key];
        }
      }
    }
  }
  return $processed;
}

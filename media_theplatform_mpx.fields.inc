<?php

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
 * Adds mapped Drupal fields to media object for insertion to MPX.
 * @param $media_object
 * @param $fields
 * @param bool $update (optional, changes the structure of the returned media object)
 * @return mixed
 */
function _mpx_build_media_object_fields($media_object, $fields, $update = false) {
  // Special handling for categories
  // @todo: This will need to be modified when multiple taxonomies are supported
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
      } else {
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
 * @param $id
 * @param $fields
 * @return bool
 */
function media_theplatform_mpx_update_fields_in_mpx($id, $fields) {
  if(media_theplatform_mpx_check_token()) {
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
<?php


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
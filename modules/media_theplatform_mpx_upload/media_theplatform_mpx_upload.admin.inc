<?php
/**
 * System settings form to set upload server
 * @param $form
 * @param $form_state
 * @return mixed
 */
function media_theplatform_mpx_form_upload_server_settings($form, $form_state) {
  $form['upload'] = array(
    '#type' => 'fieldset',
    '#title' => t('Upload Settings'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $upload_servers = media_theplatform_mpx_upload_get_servers();
  if($upload_servers) {
    $form['upload']['media_theplatform_mpx__upload_server'] = array(
      '#type' => 'select',
      '#title' => t('Upload server:'),
      '#requred' => true,
      '#empty_option' => t('- Select -'),
      '#default_value' => media_theplatform_mpx_variable_get('upload_server', 0),
      '#description' => t('Set the server which mpxMedia will be uploaded to.'),
      '#options' => $upload_servers,
    );
  }
  else {
    $form['upload']['message'] = array(
      '#markup' => t('Please select an MPX account to display a list of available upload servers'),
    );
  }

  $form['publishing'] = array(
    '#type' => 'fieldset',
    '#title' => t('Publishing Settings'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $form['publishing']['media_theplatform_mpx__auto_publish'] = array(
    '#type' => 'checkbox',
    '#title' => t('Publish nodes automatically when videos are finished publishing in mpx.'),
    '#default_value' => media_theplatform_mpx_variable_get('auto_publish', true),
  );

  return system_settings_form($form);
}

/**
 * Admin page which lists upload log with status
 * @return string
 */
function media_theplatform_mpx_upload_list() {
  $header = array('ID', 'File', 'Title', 'Created', 'Updated', 'Status', 'Status Message');
  $rows = array();

  $uploads = db_select('mpx_uploads', 'q')
    ->fields('q')
    ->orderBy('created', 'DESC')
    ->extend('PagerDefault')
    ->limit(10)
    ->execute()
    ->fetchAll();

  if(count($uploads)) {
    foreach($uploads as $upload) {
      $rows[] = array(
        $upload->id,
        $upload->file_path,
        $upload->title,
        date("Y-m-d H:i:s", $upload->created),
        date("Y-m-d H:i:s", $upload->updated),
        $upload->status,
        $upload->message
      );
    }
  }

  $output = theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No uploads tracked.')));
  $output .= theme('pager');
  return $output;
}
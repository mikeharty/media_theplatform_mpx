<?php

function media_theplatform_mpx_form_upload_server_settings($form, $form_state) {
  $form['upload'] = array(
    '#type' => 'fieldset',
    '#title' => t('Upload Settings'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $upload_servers = media_theplatform_mpx_upload_get_servers();
  if($upload_servers) {
    // Add an empty option to the front of the list
    array_unshift($upload_servers, t('- Select -'));
    $form['upload']['upload-server'] = array(
      '#type' => 'select',
      '#title' => t('Upload server:'),
      '#requred' => true,
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

  $form['publishing']['auto-publish'] = array(
    '#type' => 'checkbox',
    '#title' => t('Publish nodes automatically when videos are finished publishing in MPX.'),
    '#default_value' => media_theplatform_mpx_variable_get('upload_auto_publish', true),
  );

  return system_settings_form($form);
}
<?php
/**
 * @file
 * functions for Drupal taxonomies.
 */

/**
 * Synchronizes mpx categories with a Drupal vocabulary
 *
 * @return boolean
 *    True if successful, false on any error
 */
function media_theplatform_mpx_taxonomy_sync() {
  if(media_theplatform_mpx_variable_get('sync_vocabulary', 0)) {
    _mpx_taxonomy_sync_down();
    _mpx_taxonomy_sync_up();
  }
}

/**
 * Syncs any newly created or changed terms from Drupal to mpx
 */
function _mpx_taxonomy_sync_up() {
  // Find new terms in Drupal and create them in mpx
  $unsynced_terms = _mpx_taxonomy_get_unsynced_terms();
  foreach($unsynced_terms as $term) {
    $tid = $term->tid;
    $parent_tid = $term->parent;
    if($parent_tid)
      $mpx_parent_id = _mpx_taxonomy_get_term_by_tid($parent_tid)->mpx_id;
    else
      $mpx_parent_id = '';
    $mpx_category = _mpx_taxonomy_create_category_in_mpx($term->name, $mpx_parent_id, $term->weight);
    if($mpx_category)
      _mpx_taxonomy_store_synced_term($mpx_category->id, $mpx_parent_id, $tid, $term->parent, $term->name, $mpx_category->{'plcategory$fullTitle'}, time());
  }

  // Find terms which were updated in Drupal and update them in mpx
  $updated_terms = _mpx_taxonomy_get_updated_terms();
  foreach($updated_terms as $term) {
    $drupal_term = taxonomy_term_load($term->tid);
    $parents = array_keys(taxonomy_get_parents($term->tid));
    $parent = isset($parents[0]) ? $parents[0] : null;
    if(isset($parent) && $parent !== 0) {
      $term->parent = $parent;
      $term->mpx_parent_id = _mpx_taxonomy_get_term_by_tid($term->parent)->mpx_id;
    } else {
      $term->mpx_parent_id = '';
    }
    $term->name = $drupal_term->name;
    $term->weight = $drupal_term->weight;
    _mpx_taxonomy_update_term_in_sync_table($term->mpx_id, $term->mpx_parent_id, $term->name, $term->full_title, $term->parent);
    _mpx_taxonomy_update_category_in_mpx($term->mpx_id, $term->name, $term->mpx_parent_id, $term->weight);
  }

  // Find terms which were deleted in Drupal and delete them from mpx
  $deleted_terms = _mpx_taxonomy_get_deleted_terms();
  foreach($deleted_terms as $term) {
    _mpx_taxonomy_delete_category_in_mpx($term->mpx_id);
    _mpx_taxonomy_delete_term_from_sync_table($term->tid);
  }
}

/**
 * Syncs any newly created or changed categories from mpx to Drupal
 */
function _mpx_taxonomy_sync_down() {
  // Get a list of all current mpx categories
  if($mpx_categories = _mpx_taxonomy_get_categories()) {
    $synced = array();
    foreach($mpx_categories as $mpx_category) {
      // First look in our table for a match
      $stored_category = _mpx_taxonomy_get_term_by_mpx_id($mpx_category->id);
      // If we found a match, see if it has been updated
      if($stored_category) {
        $synced[] = $stored_category->tid;
        // If the term has been updated on both ends,
        // whichever was updated most recently will take precedence.
        if(($mpx_category->updated / 1000) > $stored_category->updated) {
          _mpx_taxonomy_update_term_in_drupal($mpx_category->{'plcategory$parentId'}, $mpx_category->title, $stored_category->tid, $mpx_category->{'plcategory$order'});
          _mpx_taxonomy_update_term_in_sync_table($stored_category->mpx_id, $mpx_category->{'plcategory$parentId'}, $mpx_category->title, $mpx_category->{'plcategory$fullTitle'}, $stored_category->parent);
        }
      } else {
        // No match, create a new term
        $synced[] = _mpx_taxonomy_create_term($mpx_category->id, $mpx_category->{'plcategory$parentId'}, $mpx_category->title, $mpx_category->{'plcategory$fullTitle'}, $mpx_category->updated / 1000, $mpx_category->{'plcategory$order'});
      }
    }
    // Delete any terms we have in our sync table that are no longer in mpx
    foreach(_mpx_taxonomy_get_all_synced_terms() as $term) {
      if(!in_array($term->tid, $synced)) {
        // Delete from Drupal
        taxonomy_term_delete($term->tid);
        // Remove from our sync table
        _mpx_taxonomy_delete_term_from_sync_table($term->tid);
      }
    }
  }
}

/**
 * Delete a term from our term tracking table
 * @param $tid
 */
function _mpx_taxonomy_delete_term_from_sync_table($tid) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  db_delete('mpx_categories')
    ->condition('tid', $tid)
    ->condition('vid', $vid)
    ->execute();
}

/**
 * Update a tracked term
 * @param $tid
 * @param $status
 */
function _mpx_taxonomy_update_term_status($tid, $status) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  db_update('mpx_categories')
    ->condition('tid', $tid, '=')
    ->condition('vid', $vid, '=')
    ->fields(array(
      'status' => $status,
    ))
    ->execute();
}

/**
 * Updates a term in our tracking table
 * @param $mpx_id
 * @param $mpx_parent_id
 * @param $title
 * @param $full_title
 * @param $parent
 * @param string $status
 */
function _mpx_taxonomy_update_term_in_sync_table($mpx_id, $mpx_parent_id, $title, $full_title, $parent, $status = '') {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  db_update('mpx_categories')
    ->condition('mpx_id', $mpx_id, '=')
    ->condition('vid', $vid, '=')
    ->fields(array(
      'mpx_parent_id' => $mpx_parent_id,
      'title' => $title,
      'parent' => $parent,
      'status' => $status,
      'full_title' => $full_title,
    ))
    ->execute();
}

/**
 * Updates an existing Drupal term with changes from MPX
 * @param $mpx_parent_id
 * @param $title
 * @param $tid
 * @param $weight
 */
function _mpx_taxonomy_update_term_in_drupal($mpx_parent_id, $title, $tid, $weight) {
  $term = taxonomy_term_load($tid);
  $term->name = $title;
  $term->parent = _mpx_taxonomy_get_term_tid_by_mpx_id($mpx_parent_id);
  $term->weight = $weight;
  taxonomy_term_save($term);
}

/**
 * Get a Drupal term tid from the tracking table by mpx ID
 * @param $mpx_id
 * @return int
 */
function _mpx_taxonomy_get_term_tid_by_mpx_id($mpx_id) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $tid = db_select('mpx_categories', 'c')
    ->fields('c', array('tid'))
    ->condition('mpx_id', $mpx_id, '=')
    ->condition('vid', $vid, '=')
    ->execute()
    ->fetchField();

    return $tid ? $tid : 0;
}

/**
 * Get a term from our tracking table by mpx ID
 * @param $mpx_id
 * @return bool
 */
function _mpx_taxonomy_get_term_by_mpx_id($mpx_id) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $term = db_select('mpx_categories', 'c')
    ->fields('c')
    ->condition('mpx_id', $mpx_id, '=')
    ->condition('vid', $vid, '=')
    ->execute()
    ->fetchAll();
    return isset($term[0]) ? $term[0] : false;
}

/**
 * Get a term from our tracking table by Drupal tid
 * @param $tid
 * @return bool
 */
function _mpx_taxonomy_get_term_by_tid($tid) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $term = db_select('mpx_categories', 'c')
    ->fields('c')
    ->condition('tid', $tid, '=')
    ->condition('vid', $vid, '=')
    ->execute()
    ->fetchAll();;
  return isset($term[0]) ? $term[0] : false;
}

/**
 * Creates a term in Drupal and stores it's details in our tracking table
 * @param $mpx_id
 * @param $mpx_parent_id
 * @param $title
 * @param $full_title
 * @param $updated
 * @param $weight
 * @return mixed
 */
function _mpx_taxonomy_create_term($mpx_id, $mpx_parent_id, $title, $full_title, $updated, $weight) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $parent_id = $mpx_parent_id ? _mpx_taxonomy_get_term_tid_by_mpx_id($mpx_parent_id) : 0;

  $term = new stdClass();
  $term->vid = $vid;
  $term->name = $title;
  $term->parent = $parent_id;
  $term->format = 'filtered_html';
  $term->description = '';
  $term->weight = $weight;
  taxonomy_term_save($term);
  $tid = $term->tid;

  _mpx_taxonomy_store_synced_term($mpx_id, $mpx_parent_id, $tid, $parent_id, $title, $full_title, $updated);

  return $tid;
}

/**
 * Stores information about a term in our tracking table
 * @param $mpx_id
 * @param $mpx_parent_id
 * @param $tid
 * @param $parent_id
 * @param $title
 * @param $full_title
 * @param $updated
 */
function _mpx_taxonomy_store_synced_term($mpx_id, $mpx_parent_id, $tid, $parent_id, $title, $full_title, $updated) {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  db_insert('mpx_categories')
    ->fields(array(
      'mpx_id' => $mpx_id,
      'mpx_parent_id' => $mpx_parent_id,
      'tid' => $tid,
      'parent' => $parent_id,
      'title' => $title,
      'full_title' => $full_title,
      'vid' => $vid,
      'updated' => $updated,
    ))
    ->execute();
}

/**
 * Gets a list of all categories in MPX and their details
 * @return bool
 */
function _mpx_taxonomy_get_categories() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = MPX_MD_BASE.'/media/data/Category?range=1-10000&token='.$mpx_token.'&account='.$account_id.'&schema=1.2.0&form=json';
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
    // Check for valid JSON
    $response = media_theplatform_mpx_check_json_response($response, 'getPublishingProfiles');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      return false;
    }
    else {
      $categories = $response['response']->entries;
      // Sort the categories by level so that parents are always created before children
      usort($categories, "_mpx_sort_categories_by_level");
      return $categories;
    }
  } else {
    return false;
  }
}

/**
 * Sorts an array of categories by level
 * This is used to ensure parent level terms are always created before children
 * @param $a
 * @param $b
 * @return int
 */
function _mpx_sort_categories_by_level($a, $b) {
  if($a->{'plcategory$level'} == $b->{'plcategory$level'})
    return 0;

  return ($a->{'plcategory$level'} < $b->{'plcategory$level'}) ? -1 : 1;
}

/**
 * Sorts an array of Drupal terms by depth
 * @param $a
 * @param $b
 * @return int
 */
function _mpx_sort_terms_by_depth($a, $b) {
  if($a->depth == $b->depth)
    return 0;

  return ($a->depth < $b->depth) ? -1 : 1;
}

/**
 * This gets a list of unsynced terms and creates a tree from
 * them which includes their parent id and depth, sorted by depth.
 * This code uses modified code from taxonomy_get_tree, taxonomy.module
 * @return array|bool
 */
function _mpx_taxonomy_get_unsynced_terms() {
  if($vid = media_theplatform_mpx_variable_get('sync_vocabulary', 0)){
    $synced_terms = db_select('mpx_categories', 'c')
      ->fields('c', array('tid'))
      ->condition('vid', $vid, '=');
    $unsynced_terms = db_select('taxonomy_term_data', 't');
    $unsynced_terms->join('taxonomy_term_hierarchy', 'h', 'h.tid = t.tid');
    $unsynced_terms = $unsynced_terms->fields('t')
      ->fields('h', array('parent'))
      ->condition('t.tid', $synced_terms, 'NOT IN')
      ->execute()
      ->fetchAll();

    foreach($unsynced_terms as &$term) {
      $term->depth = _mpx_taxonomy_get_term_depth($term->tid);
    }

    usort($unsynced_terms, "_mpx_sort_terms_by_depth");
    return $unsynced_terms;
  }
  return false;
}

/**
 * Gets all terms marked as updated from our tracking table
 * @return mixed
 */
function _mpx_taxonomy_get_updated_terms() {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $terms = db_select('mpx_categories', 'c')
    ->fields('c')
    ->condition('vid', $vid, '=')
    ->condition('status', 'updated', '=')
    ->execute()
    ->fetchAll();

  return $terms;
}

/**
 * Gets all terms marked as deleted from our tracking table
 * @return mixed
 */
function _mpx_taxonomy_get_deleted_terms() {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $terms = db_select('mpx_categories', 'c')
    ->fields('c')
    ->condition('vid', $vid, '=')
    ->condition('status', 'deleted', '=')
    ->execute()
    ->fetchAll();

  return $terms;
}

/**
 * Gets all terms from our tracking table
 * @return mixed
 */
function _mpx_taxonomy_get_all_synced_terms() {
  $vid = media_theplatform_mpx_variable_get('sync_vocabulary');
  $terms = db_select('mpx_categories', 'c')
    ->fields('c')
    ->condition('vid', $vid, '=')
    ->execute()
    ->fetchAll();

  return $terms;
}

/**
 * Gets the depth for a Drupal term by tid
 * @param $tid
 * @return int
 */
function _mpx_taxonomy_get_term_depth($tid) {
  $depth = 0;
  while ($parent = db_select('taxonomy_term_hierarchy', 't') ->condition('tid', $tid, '=') ->fields('t') ->execute() ->fetchAssoc()) {
    $depth++;
    $tid = $parent['parent'];
  }
  return $depth;
}

/**
 * Creates a new category in mpx
 * @param $term
 * @param $parent_id
 * @param $weight
 * @return bool
 */
function _mpx_taxonomy_create_category_in_mpx($term, $parent_id, $weight) {
  if(media_theplatform_mpx_check_token()) {
    $payload = new stdClass;
    $payload->{'$xmlns'}->plcategory = "http://xml.theplatform.com/media/data/Category";
    $payload->locked = false;
    $payload->{'plcategory$order'} = $weight;
    $payload->{'plcategory$parentId'} = $parent_id;
    $payload->{'plcategory$titleLocalized'} = new stdClass;
    $payload->title = $term;
    $payload = json_encode($payload);

    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = MPX_MD_BASE.'/media/data/Category?token='.$mpx_token.'&account='.$account_id.'&fields=title%2Cplcategory%24fullTitle%2Cplcategory%24parentId%2Cplcategory%24order%2Cversion%2Clocked%2Cid%2Cguid%2CownerId&schema=1.2.0';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: text/plain'));
    curl_setopt($ch, CURLOPT_POST, 1);
    curl_setopt($ch, CURLOPT_POSTFIELDS, $payload);

    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    // Check for valid cURL response
    $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      watchdog('media_theplatform_mpx', $responseCode['response'], array(), WATCHDOG_ERROR);
      return false;
    }
    // Check for valid JSON
    $response = media_theplatform_mpx_check_json_response($response, 'createCategory');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      watchdog('media_theplatform_mpx', $response['response'], array(), WATCHDOG_ERROR);
      return false;
    }
    else {
      return $response['response'];
    }
  } else {
    return false;
  }
}

/**
 * Updates an existing category in mpx
 * @param $term_id
 * @param $term_name
 * @param $parent_id
 * @param $weight
 * @return bool
 */
function _mpx_taxonomy_update_category_in_mpx($term_id, $term_name, $parent_id, $weight) {
  if(media_theplatform_mpx_check_token()) {
    $payload = new stdClass;
    $payload->{'$xmlns'}->plcategory = "http://xml.theplatform.com/media/data/Category";
    $payload->id = $term_id;
    $payload->{'plcategory$order'} = $weight;
    $payload->{'plcategory$titleLocalized'} = new stdClass;
    $payload->{'plcategory$parentId'} = $parent_id;
    $payload->title = $term_name;
    $payload = json_encode($payload);

    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = MPX_MD_BASE.'/media/data/Category?token='.$mpx_token.'&account='.$account_id.'&method=put&fields=title%2Cplcategory%24titleLocalized%2Cplcategory%24parentId%2Cplcategory%24order%2Cversion%2Clocked%2Cid%2Cguid%2CownerId&schema=1.2.0';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: text/plain'));
    curl_setopt($ch, CURLOPT_POST, 1);
    curl_setopt($ch, CURLOPT_POSTFIELDS, $payload);


    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    // Check for valid cURL response
    $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      watchdog('media_theplatform_mpx', $responseCode['response'], array(), WATCHDOG_ERROR);
      return false;
    }
    // Check for valid JSON
    $response = media_theplatform_mpx_check_json_response($response, 'updateCategory');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      watchdog('media_theplatform_mpx', $response['response'], array(), WATCHDOG_ERROR);
      return false;
    }
    else {
      return $response['response'];
    }
  } else {
    return false;
  }
}

/**
 * Deletes a category in mpx
 * @param $mpx_id
 * @return bool
 */
function _mpx_taxonomy_delete_category_in_mpx($mpx_id) {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = $mpx_id.'?token='.$mpx_token.'&account='.$account_id.'&method=delete&schema=1.2.0&form=json';
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
      watchdog('media_theplatform_mpx', $responseCode['response'], array(), WATCHDOG_ERROR);
      return false;
    }
    // Check for valid JSON
    $response = media_theplatform_mpx_check_json_response($response, 'deleteCategory');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      watchdog('media_theplatform_mpx', $response['response'], array(), WATCHDOG_ERROR);
      return false;
    }
    else {
      return $response['response'];
    }
  } else {
    return false;
  }
}

/**
 * Checks whether a provided vocabulary is empty
 * @param $vid
 * @return bool
 */
function _mpx_taxonomy_check_empty_taxonomy($vid) {
  $tree = taxonomy_get_tree($vid);
  if(count($tree))
    return false;
  else
    return true;
}

/**
 * Checks whether there are any categories in mpx
 * @return bool
 */
function _mpx_taxonomy_check_empty_mpx_categories() {
  $categories = _mpx_taxonomy_get_categories();
  if(count($categories))
    return false;
  else
    return true;
}
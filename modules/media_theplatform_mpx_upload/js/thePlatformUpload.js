var thePlatformUpload = {};

(function($) {
  thePlatformUpload = {

    // @todo: add support for EU/other regions
    urls: {
      media_data: 'http://data.media.theplatform.com',
      file_mgmt: 'http://fms.theplatform.com'
    },

    uploadedFragments: 0,
    uploadAttempts: 0,
    percentUploaded: 0,
    statusElement: null,
    callback: null,
    fragSize: 1024 * 1024 * 5,

    /**
     * Wrapper which sets variables and kicks off upload process
     * @param title
     * @param fields
     * @param fileElementId
     * @param accountId
     * @param token
     * @param uploadServerId
     * @param statusCallback
     * @param finishCallback
     * @param errorCallback
     */
    uploadFile: function(title, fields, fileElementId, accountId, token, uploadServerId, statusCallback, finishCallback, errorCallback) {
      this.token = token;
      this.accountId = accountId;
      this.uploadServerId = uploadServerId;
      this.statusCallback = statusCallback;
      this.finishCallback = finishCallback;
      this.errorCallback = errorCallback;
      this.fileElementId = fileElementId;
      this.uploadedFragments = 0;
      this.uploadAttempts = 0;
      this.percentUploaded = 0;
      this.file = document.getElementById( this.fileElementId ).files[0];
      this.fileName = $('#'+this.fileElementId).val().split('/').pop().split('\\').pop();
      this.fragments = this.fragFile(this.file);
      this.title = title;
      this.fields = fields;

      var me = this;
      me.getFormat().then(function() {
        return me.createMediaObject();
      }).then(function() {
        return me.getUploadServerURL();
      }).then(function() {
        return me.startUpload();
      }).then(function() {
        return me.uploadFragments();
      }).then(
        // Succeeded
        function() {
          me.finishUpload();
      },
        // Failed
        function() {
          me.cancelUpload();
      }).catch(function(e) {
        console.log(e);
      });
    },

    /**
     @function fragFile Slices a file into fragments
     @param {File} file - file to slice
     @return {Array} array of file fragments
     */
    fragFile: function( file ) {
      var fragSize = this.fragSize;
      var i, j, k;
      var ret = [ ];

      File.prototype.slice = File.prototype.webkitSlice || File.prototype.mozSlice || File.prototype.slice;

      if ( !( File.prototype.slice ) ) {
        return this.file;
      }

      for ( i = j = 0, k = Math.ceil( this.file.size / fragSize ); 0 <= k ? j < k : j > k; i = 0 <= k ? ++j : --j ) {
        ret.push( this.file.slice( i * fragSize, ( i + 1 ) * fragSize ) );
      }

      return ret;
    },

    /**
     * Creates a new Media Object in MPX
     * @param params - Parameter object
     */
    createMediaObject: function() {
      var me = this;
      var promise = new RSVP.Promise(function(resolve, reject){
        me.statusCallback('Creating new media object in MPX.');
        var mediaObj = {
          $xmlns: {
            media: 'http://search.yahoo.com/mrss/',
            pl: 'http://xml.theplatform.com/data/object',
            plmedia: 'http://xml.theplatform.com/media/data/Media',
            pl1: me.accountId
          },
          entries: [
            me.fields
          ]
        };

        mediaObj.entries[0].plmedia$approved = true;

        var url = me.urls.media_data + '/media/data/Media/list?' +
          'schema=1.2' +
          '&form=json' +
          '&account=' + encodeURIComponent(me.accountId) +
          '&token=' + encodeURIComponent(me.token);

        $.ajax({
          type: 'POST',
          url: url,
          data: JSON.stringify(mediaObj),
          success: function(data){
            data = JSON.parse(data);
            if(data.hasOwnProperty('entries')) {
              me.statusCallback('Media object created.');
              data = {
                id: data.entries[0].id,
                guid: data.entries[0].guid
              };
              me.mediaObj = data;
              resolve();
            } else {
              me.statusCallback('Error occurred while creating media object: ' + JSON.stringify(data));
              me.errorCallback();
              reject();
            }
          },
          error: function(data){
            me.statusCallback('Unable to reach media data server, please try again later.');
            me.errorCallback();
            reject();
          },
          contentType: 'text/plain'
        });
      });

      return promise;
    },

    /**
     * Fetches a random upload server from the load balancer
     */
    getUploadServerURL: function() {
      var me = this;
      var promise = new RSVP.Promise(function(resolve, reject){
        me.statusCallback('Finding random upload server.');
        var url = me.urls.file_mgmt+'/web/FileManagement/getUploadUrls?' +
          'form=JSON' +
          '&schema=1.4' +
          '&token=' + encodeURIComponent(me.token) +
          '&account=' + encodeURIComponent(me.accountId) +
          '&_serverId=' + encodeURIComponent(me.uploadServerId);

        $.ajax({
          type: 'GET',
          url: url,
          success: function(data){
            data = JSON.parse(data);
            if(data.hasOwnProperty('getUploadUrlsResponse')) {
              me.statusCallback('Upload server found.');
              data = data.getUploadUrlsResponse[Math.floor(Math.random() * data.getUploadUrlsResponse.length)];
              me.uploadServerURL = data;
              resolve();
            } else {
              me.statusCallback('Error while finding upload server: ' + JSON.stringify(data));
              me.errorCallback();
              reject();
            }
          },
          error: function(data){
            me.statusCallback('Error while finding upload server: ' + JSON.stringify(data));
            me.errorCallback();
            reject();
          },
          contentType: 'text/plain'
        });
      });

      return promise;
    },


    /**
     * Method that issues an ajax call to determine the title associated with
     * the file format then starts the upload file callback process on success.
     */
    getFormat: function() {
      var me = this;
      var promise = new RSVP.Promise(function(resolve, reject){
        me.statusCallback('Fetching format information...');
        // Extract the extension from the filename.
        var pieces = me.fileName.split(".");
        var ext = pieces.pop();
        var url = '/admin/config/media/theplatform/mpx-formats/' + ext;

        $.ajax({
            type: 'POST',
            url: url,
            success: function(data){
                data = JSON.parse(data);
                me.fileExt = data.response;
                me.statusCallback('Received format information.');
                resolve();
            },
            error: function(data){
                me.statusCallback('Error while finding upload server: ' + JSON.stringify(data));
                me.errorCallback();
                reject();
            }
        });
      });

      return promise;
   },

    /**
     * Initiates an upload with the upload server
     */
    startUpload: function() {
      var me = this;
      var promise = new RSVP.Promise(function(resolve, reject){
        me.statusCallback('Initiating upload session with upload server.');
        var url = me.uploadServerURL + '/web/Upload/startUpload?' +
          'schema=1.1' +
          '&token=' + encodeURIComponent(me.token) +
          '&account=' + encodeURIComponent(me.accountId) +
          '&_guid=' + me.mediaObj.guid +
          '&_mediaId=' + me.mediaObj.id +
          '&_filePath=' + me.fileName +
          '&_fileSize='+ me.file.size +
          '&_mediaFileInfo.format=' + me.fileExt +
          '&_serverId=' + encodeURIComponent(me.uploadServerId);

        $.ajax({
          url: url,
          type: "PUT",
          xhrFields: {
            withCredentials: true
          },
          success: function(data){
            me.statusCallback('Session established with upload server.');
            resolve();
          },
          error: function(data){
            // If we got a response, display the error
            if(data.status == 200) {
              me.statusCallback('Session established with upload server.');
              resolve();
            } else {
              me.statusCallback('Unable to reach upload server, please try again later.');
              me.errorCallback();
              reject();
            }
          }
        });
      });

      return promise;
    },

    /**
     * Uploads the file to the upload server in 5MB fragments
     */
    uploadFragments: function() {
      var me = this;
      var promise = new RSVP.Promise(function(resolve, reject){
        var uploadFragment = function(me) {
          me.statusCallback('Uploading fragment ' + (me.uploadedFragments + 1) + ' of '
            + me.fragments.length + ' - 0%<br />' + me.percentUploaded + '% of total uploaded.');

          var fragments = me.fragments;

          var url = me.uploadServerURL + '/web/Upload/uploadFragment?';
              url += 'schema=1.1';
              url += '&token=' + encodeURIComponent(me.token);
              url += '&account=' + encodeURIComponent(me.accountId);
              url += '&_guid=' + me.mediaObj.guid;
              url += '&_offset=' + me.uploadedFragments*me.fragSize;
              url += '&_size=' + fragments[me.uploadedFragments].size;

          $.ajax( {
            url: url,
            processData: false,
            data: fragments[me.uploadedFragments],
            type: "PUT",
            dataType: "text",
            xhrFields: {
              withCredentials: true
            },
            xhr: function() {
              var xhr = new window.XMLHttpRequest();
              //Upload progress
              xhr.upload.addEventListener("progress", function(evt){
                if (evt.lengthComputable) {
                  var fragmentPercent = Math.round((evt.loaded / evt.total)*100);
                  me.percentUploaded = Math.round(((me.uploadedFragments + fragmentPercent/100) / me.fragments.length)*100);
                  me.statusCallback('Uploading fragment ' + (me.uploadedFragments + 1) + ' of '
                    + me.fragments.length + ' - ' + fragmentPercent + '%<br />'+me.percentUploaded+'% of total uploaded.');
                }
              }, false);
              return xhr;
            },
            success: function(data){
              me.statusCallback('Fragment ' + (me.uploadedFragments + 1) + ' of ' + me.fragments.length + ' successfully uploaded.');
              if(me.uploadedFragments < fragments.length-1) {
                me.uploadedFragments++;
                // reset the upload attempts, we'll allow 5 tries for any fragment
                me.uploadAttempts = 0;
                uploadFragment(me);
              } else {
                me.statusCallback('All fragments uploaded.');
                resolve();
              }
            },
            error: function(data){
              // If we got a response, display the error
              if(data.responseCode == 200 && me.uploadedFragments == fragments.length-1) {
                me.statusCallback('All fragments uploaded.');
              } else {
                if(me.uploadAttempts == 15) {
                  me.statusCallback('Unable to upload fragment after 5 tries. Please try again later.');
                  me.errorCallback();
                  reject();
                } else {
                  me.uploadAttempts++;
                  me.statusCallback('Upload server not yet ready, please wait...');
                  setTimeout(function(){uploadFragment(me)}, 6000);
                }
              }
            }
          });
        };

        uploadFragment(me);
      });

      return promise;
    },

    /**
     * Finalizes the upload with the upload server, then calls the finishCallback.
     */
    finishUpload: function() {
      this.statusCallback('Finalizing upload.');
      var url = this.uploadServerURL + '/web/Upload/finishUpload?';
          url += 'schema=1.1';
          url += '&token=' + encodeURIComponent(this.token);
          url += '&account=' + encodeURIComponent(this.accountId);
          url += '&_guid=' + this.mediaObj.guid;

      var data = "finished";
      var me = this;

      $.ajax({
        url: url,
        data: data,
        type: "POST",
        xhrFields: {
          withCredentials: true
        },
        success: function( data ) {
          // clear the fragments from memory
          me.fragments = null;
          me.statusCallback('Upload completed.');
          me.finishCallback(me.mediaObj.guid, me.mediaObj.id);
        },
        //@todo: add code to handle timeout and retry
        error: function( data ) {
          if(data.status == 200) {
            me.statusCallback('Upload completed.');
            me.finishCallback(me.mediaObj.guid, me.mediaObj.id);
          } else {
            me.statusCallback('Failed to complete upload. Response: ' + JSON.stringify(data));
            me.errorCallback();
          }
        }
      });
    },

    cancelUpload: function() {
      this.statusCallback('Cancelling upload...');
      var url = this.uploadServerURL + '/web/Upload/cancelUpload?';
          url += 'schema=1.1';
          url += '&token=' + encodeURIComponent(this.token);
          url += '&account=' + encodeURIComponent(this.accountId);
          url += '&_guid=' + this.mediaObj.guid;

      var me = this;

      $.ajax({
        url: url,
        type: "PUT",
        xhrFields: {
          withCredentials: true
        },
        success: function( data ) {
          me.statusCallback('Upload cancelled due to error uploading fragments. Please try again later.');
          me.finishCallback(null);
        },
        error: function( data ) {
          if(data.status == 200) {
            me.statusCallback('Upload cancelled due to error uploading fragments. Please try again later.');
            me.finishCallback(null);
            // Don't re-enable the submit button in this case.
          } else {
            me.statusCallback('Failed to cancel upload. Response: ' + JSON.stringify(data));
            me.errorCallback();
          }
        }
      });

    }
  }
}(jq1110));

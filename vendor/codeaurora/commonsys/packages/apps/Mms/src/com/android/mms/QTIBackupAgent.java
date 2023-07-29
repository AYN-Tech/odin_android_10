/*
Copyright (c) 2014, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
package com.android.mms;

import android.app.backup.BackupAgentHelper;
import android.app.backup.FileBackupHelper;
import android.app.backup.FullBackupDataOutput;
import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.File;
import java.io.IOException;


public class QTIBackupAgent extends BackupAgentHelper {
    static final private String TAG = "QTIBackupAgent";

    static final String SMS = "smsBackup";
    static final String MMS = "mmsBackup";

    @Override
        public void onFullBackup(FullBackupDataOutput data) throws IOException {
            QTIBackupSMS smsBackup = new QTIBackupSMS(this, SMS);
            smsBackup.performBackup();
            QTIBackupMMS mmsBackup = new QTIBackupMMS(this, MMS);
            mmsBackup.performBackup();
            super.onFullBackup(data);
        }

    @Override
        public void onRestoreFile(ParcelFileDescriptor data, long size,
                File destination, int type, long mode, long mtime)
        throws IOException {
            super.onRestoreFile(data, size, destination, type, mode, mtime);
            if(destination.getName().equalsIgnoreCase(MMS)){
                QTIBackupMMS mmsBackup = new QTIBackupMMS(this, MMS);
                mmsBackup.performRestore();
            }
            if(destination.getName().equalsIgnoreCase(SMS)){
                QTIBackupSMS smsBackup = new QTIBackupSMS(this, SMS);
                smsBackup.performRestore();
            }
        }
}

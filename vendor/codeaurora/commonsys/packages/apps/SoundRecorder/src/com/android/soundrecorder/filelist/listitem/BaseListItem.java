/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.soundrecorder.filelist.listitem;

public abstract class BaseListItem {
    public static final int SUPPORT_DELETE = 1 << 0;
    public static final int SUPPORT_SHARE = 1 << 1;
    public static final int SUPPORT_EDIT = 1 << 2;
    public static final int SUPPORT_NONE = 0;
    public static final int SUPPORT_ALL = 0xffffffff;

    protected long mId;
    protected String mTitle;
    protected boolean mChecked;
    protected String mPath;
    protected boolean mSelectable = true;

    protected int mItemType;
    public static final int TYPE_NONE = 0;
    public static final int TYPE_FOLDER = 1;
    public static final int TYPE_MEDIA_ITEM = 2;

    protected int mSupportedOperation;

    public long getId() {
        return mId;
    }

    public void setId(long id) {
        mId = id;
    }

    public String getTitle() {
        return mTitle;
    }

    public void setTitle(String title) {
        mTitle = title;
    }

    public String getPath() {
        return mPath;
    }

    public void setPath(String path) {
        mPath = path;
    }

    public boolean isChecked() {
        return mSelectable && mChecked;
    }

    public void setChecked(boolean checked) {
        mChecked = checked;
    }

    public int getItemType() {
        return mItemType;
    }

    protected void setItemType(int itemType) {
        mItemType = itemType;
    }

    public boolean isSelectable() {
        return mSelectable;
    }

    protected void setSelectable(boolean selectable) {
        mSelectable = selectable;
    }

    public int getSupportedOperation() {
        return mSupportedOperation;
    }

    protected void setSupportedOperation(int operation) {
        mSupportedOperation = operation;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof BaseListItem) {
            return getId() == ((BaseListItem) o).getId();
        } else {
            return super.equals(o);
        }
    }

    public void copyFrom(BaseListItem item) {
        setTitle(item.getTitle());
        setPath(item.getPath());
        setItemType(item.getItemType());
        setSelectable(item.isSelectable());
        setSupportedOperation(item.getSupportedOperation());
        setChecked(item.isChecked());
    }
}

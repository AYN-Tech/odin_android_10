/*
 * Copyright (c) 2014, 2016-2017, The Linux Foundation. All rights reserved.
 *
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
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.ui;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.content.Context;
import android.content.res.Configuration;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.GridView;
import android.widget.SimpleAdapter;

import com.android.mms.MmsConfig;
import com.android.mms.R;

public class AttachmentPagerAdapter extends PagerAdapter {
    public static final int GRID_COLUMN_COUNT = 3;
    public static final int PAGE_GRID_COUNT   = 6;
    public static final int GRID_ITEM_HEIGHT  = 91;

    public static final int ADD_SUBJECT          = 0;
    public static final int ADD_IMAGE            = 1;
    public static final int TAKE_PICTURE         = 2;
    public static final int ADD_VIDEO            = 3;
    public static final int RECORD_VIDEO         = 4;
    public static final int ADD_SOUND            = 5;
    public static final int RECORD_SOUND         = 6;
    public static final int ADD_SLIDESHOW        = 7;
    public static final int ADD_TEMPLATE         = 8;
    public static final int ADD_CONTACT_AS_TEXT  = 9;
    public static final int ADD_CONTACT_AS_VCARD = 10;

    private static final String GRID_ITEM_IMAGE = "grid_item_image";
    private static final String GRID_ITEM_TEXT  = "grid_item_text";
    private static final int PAGE_COUNT = 2;

    private static final int RECORD_SOUND_ITEM_POSITION    = 0;
    private static final int SLIDESHOW_ITEM_POSITION       = 1;
    private static final int IMPORT_TEMPLATE_POSITION      = 2;
    private static final int CONTACT_INFO_ITEM_POSITION    = 3;
    private static final int VCARD_ITEM_POSITION           = 4;

    private static final int ICON_LIST_SIZE = 10;

    private boolean mHasAttachment;
    private boolean mHasVcard;
    private boolean mHasSlideshow;
    private boolean mIsReplace;
    private boolean mShowAddSubject;
    private Context mContext;
    private ArrayList<GridView> mPagerGridViewViews;
    private OnItemClickListener mGridItemClickListener;
    private boolean mIsRTL = false;

    public AttachmentPagerAdapter(Context context) {
        mContext = context;
    }

    @Override
    public Object instantiateItem(ViewGroup view, int position) {
        View pagerContent = LayoutInflater.from(mContext).inflate(
                R.layout.attachment_selector_pager, view, false);
        mIsRTL = (view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL);
        bindPagerView(pagerContent, position);
        view.addView(pagerContent);
        return pagerContent;
    }

    private void bindPagerView(View pagerContent, int position) {
        GridView gridView = (GridView) pagerContent.findViewById(R.id.attachment_pager_grid);
        gridView.setAdapter(new AttachmentGridAdapter(mContext, getGridData(position),
                R.layout.attachment_selector_grid,
                new String[] {GRID_ITEM_IMAGE, GRID_ITEM_TEXT},
                new int[] {R.id.attachment_selector_image, R.id.attachment_selector_text},
                position));

        Configuration configuration = mContext.getResources().getConfiguration();
        gridView.setNumColumns((configuration.orientation == configuration.ORIENTATION_PORTRAIT)
                ? GRID_COLUMN_COUNT : GRID_COLUMN_COUNT * 2);
        gridView.setOnItemClickListener(mGridItemClickListener);
        setPagerGridViews(gridView);
    }

    private ArrayList<HashMap<String, Object>> getGridData(int pagerPosition) {
        List<IconListItem> attachmentDataList = getAttachmentData();
        ArrayList<HashMap<String, Object>> gridData = new ArrayList<HashMap<String, Object>>();
        if (mIsRTL) {
            if (pagerPosition == 0) {
                for (int i = PAGE_GRID_COUNT; i < attachmentDataList.size(); i++) {
                    IconListItem item = (IconListItem) attachmentDataList.get(i);
                       HashMap<String, Object> map = new HashMap<String, Object>();
                    map.put(GRID_ITEM_IMAGE, item.getResource());
                    map.put(GRID_ITEM_TEXT, item.getTitle());
                    gridData.add(map);
                }
            } else {
                for (int i = 0; i < PAGE_GRID_COUNT; i++) {
                    IconListItem item = (IconListItem) attachmentDataList.get(i);
                    HashMap<String, Object> map = new HashMap<String, Object>();
                    map.put(GRID_ITEM_IMAGE, item.getResource());
                    map.put(GRID_ITEM_TEXT, item.getTitle());
                    gridData.add(map);
                }
            }
            return gridData;
        }

        if (pagerPosition == 0) {
            for (int i = 0; i < PAGE_GRID_COUNT; i++) {
                IconListItem item = (IconListItem) attachmentDataList.get(i);
                HashMap<String, Object> map = new HashMap<String, Object>();
                map.put(GRID_ITEM_IMAGE, item.getResource());
                map.put(GRID_ITEM_TEXT, item.getTitle());
                gridData.add(map);
            }
        } else {
            for (int i = PAGE_GRID_COUNT; i < attachmentDataList.size(); i++) {
                IconListItem item = (IconListItem) attachmentDataList.get(i);
                HashMap<String, Object> map = new HashMap<String, Object>();
                map.put(GRID_ITEM_IMAGE, item.getResource());
                map.put(GRID_ITEM_TEXT, item.getTitle());
                gridData.add(map);
            }
        }
        return gridData;
    }

    private List<IconListItem> getAttachmentData() {
        List<IconListItem> list = new ArrayList<IconListItem>(ICON_LIST_SIZE);
        list.add(new IconListItem(mContext.getString(R.string.attach_subject),
                (!mIsReplace && mShowAddSubject) ? R.drawable.ic_attach_subject_holo_light
                        : R.drawable.ic_attach_subject_disable));
        list.add(new IconListItem(mContext.getString(R.string.attach_image),
                (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_picture_disable
                        : R.drawable.ic_attach_picture_holo_light));
        list.add(new IconListItem(mContext.getString(R.string.attach_take_photo),
                (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_capture_picture_disable
                        : R.drawable.ic_attach_capture_picture_holo_light));
        list.add(new IconListItem(mContext.getString(R.string.attach_video),
                (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_video_disable
                        : R.drawable.ic_attach_video_holo_light));
        list.add(new IconListItem(mContext.getString(R.string.attach_record_video),
                (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_capture_video_disable
                        : R.drawable.ic_attach_capture_video_holo_light));
        if (MmsConfig.getAllowAttachAudio()) {
            list.add(new IconListItem(mContext.getString(R.string.attach_sound),
                    (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_audio_disable
                            : R.drawable.ic_attach_audio_holo_light));
        }
        list.add(new IconListItem(mContext.getString(R.string.attach_record_sound),
                (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_capture_audio_disable
                        : R.drawable.ic_attach_capture_audio_holo_light));
        list.add(new IconListItem(mContext.getString(R.string.attach_slideshow),
                (!mIsReplace && mHasVcard) ? R.drawable.ic_attach_slideshow_disable
                        : R.drawable.ic_attach_slideshow_holo_light));
        boolean config_vcard = mContext.getResources().getBoolean(R.bool.config_vcard);
        if (config_vcard) {
            list.add(new IconListItem(mContext.getString(R.string.attach_add_contact_as_text),
                    (!mIsReplace && mHasSlideshow) ? R.drawable.ic_attach_contact_info_disable
                            : R.drawable.ic_attach_contact_info_holo_light));
            list.add(new IconListItem(mContext.getString(R.string.attach_add_contact_as_vcard),
                    (!mIsReplace && mHasAttachment) ? R.drawable.ic_attach_vcard_disable
                            : R.drawable.ic_attach_vcard_holo_light));

        }
        list.add(new IconListItem(mContext.getString(R.string.import_message_template),
                (!mIsReplace && mHasSlideshow) ? R.drawable.ic_attach_template_disalbe
                        : R.drawable.ic_attach_template_holo_light));

        return list;
    }

    public void setGridItemClickListener(OnItemClickListener l) {
        mGridItemClickListener = l;
    }

    public void setExistAttachmentType(boolean hasAttachment, boolean hasVcard,
            boolean hasSlideshow, boolean isReplace, boolean showAddSubject) {
        mHasAttachment = hasAttachment;
        mHasVcard = hasVcard;
        mHasSlideshow = hasSlideshow;
        mIsReplace = isReplace;
        mShowAddSubject = showAddSubject;
    }

    private void setPagerGridViews(GridView gridView) {
        if (mPagerGridViewViews == null) {
            mPagerGridViewViews = new ArrayList<GridView>();
        }
        if (!mPagerGridViewViews.contains(gridView)) {
            mPagerGridViewViews.add(gridView);
        }
    }

    public ArrayList<GridView> getPagerGridViews() {
        return mPagerGridViewViews;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        ((ViewPager) container).removeView((View) object);
    }

    @Override
    public int getCount() {
        return PAGE_COUNT;
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return view.equals(object);
    }

    @Override
    public int getItemPosition(Object object) {
        return super.getItemPosition(object);
    }

    public class IconListItem {
        private final String mTitle;
        private final int mResource;

        public IconListItem(String title, int resource) {
            mResource = resource;
            mTitle = title;
        }

        public String getTitle() {
            return mTitle;
        }

        public int getResource() {
            return mResource;
        }
    }

    public class AttachmentGridAdapter extends SimpleAdapter {
        int mCurrentPager;

        public AttachmentGridAdapter(Context context, List<? extends Map<String, ?>> data,
                int resource, String[] from, int[] to, int currentPager) {
            super(context, data, resource, from, to);
            mCurrentPager = currentPager;
        }

        @Override
        public boolean isEnabled(int position) {
            if (mCurrentPager == 0) {
                if ((!mIsReplace && mHasVcard && position != ADD_SUBJECT)
                        || shouldDisableAddSubjectItem(position)) {
                    return false;
                }
            } else {
                if (!mIsReplace && ((mHasVcard && position == RECORD_SOUND_ITEM_POSITION)
                        || (mHasVcard && position == SLIDESHOW_ITEM_POSITION)
                        || (mHasSlideshow && position == CONTACT_INFO_ITEM_POSITION)
                        || (mHasSlideshow && position == IMPORT_TEMPLATE_POSITION)
                        || (mHasAttachment && position == VCARD_ITEM_POSITION))) {
                    return false;
                }
            }
            return super.isEnabled(position);
        }

        private boolean shouldDisableAddSubjectItem(int position) {
            return position == ADD_SUBJECT && !(!mIsReplace && mShowAddSubject);
        }
    }
}

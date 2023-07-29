/*
 * Copyright (C) 2008 Esmertec AG.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.mms.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.android.mms.LogTag;
import com.android.mms.model.AudioModel;
import com.android.mms.model.ImageModel;
import com.android.mms.model.LayoutModel;
import com.android.mms.model.MediaModel;
import com.android.mms.model.MediaModel.MediaAction;
import com.android.mms.model.Model;
import com.android.mms.model.RegionMediaModel;
import com.android.mms.model.RegionModel;
import com.android.mms.model.SlideModel;
import com.android.mms.model.SlideshowModel;
import com.android.mms.model.TextModel;
import com.android.mms.model.VcardModel;
import com.android.mms.model.VideoModel;
import com.android.mms.R;
import com.android.mms.ui.AdaptableSlideViewInterface.OnSizeChangedListener;
import com.android.mms.util.ItemLoadedCallback;
import com.android.mms.util.ItemLoadedFuture;
import com.android.mms.util.ThumbnailManager.ImageLoaded;
import android.net.Uri;
import android.text.TextUtils;

/**
 * A basic presenter of slides.
 */
public class SlideshowPresenter extends Presenter {
    private static final String TAG = LogTag.TAG;
    private static final boolean DEBUG = false;
    private static final boolean LOCAL_LOGV = false;

    private Context mContext;
    private ItemLoadedFuture mItemLoadedFuture;

    protected int mLocation;
    protected final int mSlideNumber;

    protected float mWidthTransformRatio = 1.0f;
    protected float mHeightTransformRatio = 1.0f;

    // Since only the original thread that created a view hierarchy can touch
    // its views, we have to use Handler to manage the views in the some
    // callbacks such as onModelChanged().
    protected final Handler mHandler = new Handler();

    public SlideshowPresenter(Context context, ViewInterface view, Model model) {
        super(context, view, model);
        mContext = context;
        mLocation = 0;
        mSlideNumber = ((SlideshowModel) mModel).size();

        if (view instanceof AdaptableSlideViewInterface) {
            ((AdaptableSlideViewInterface) view).setOnSizeChangedListener(
                    mViewSizeChangedListener);
        }
    }

    private final OnSizeChangedListener mViewSizeChangedListener =
        new OnSizeChangedListener() {
        public void onSizeChanged(int width, int height) {
            LayoutModel layout = ((SlideshowModel) mModel).getLayout();
            mWidthTransformRatio = getWidthTransformRatio(
                    width, layout.getLayoutWidth());
            mHeightTransformRatio = getHeightTransformRatio(
                    height, layout.getLayoutHeight());
            // The ratio indicates how to reduce the source to match the View,
            // so the larger one should be used.
            float ratio = mWidthTransformRatio > mHeightTransformRatio ?
                    mWidthTransformRatio : mHeightTransformRatio;
            mWidthTransformRatio = ratio;
            mHeightTransformRatio = ratio;
            if (LOCAL_LOGV) {
                Log.v(TAG, "ratio_w = " + mWidthTransformRatio
                        + ", ratio_h = " + mHeightTransformRatio);
            }
        }
    };

    private float getWidthTransformRatio(int width, int layoutWidth) {
        if (width > 0) {
            return (float) layoutWidth / (float) width;
        }
        return 1.0f;
    }

    private float getHeightTransformRatio(int height, int layoutHeight) {
        if (height > 0) {
            return (float) layoutHeight / (float) height;
        }
        return 1.0f;
    }

    private int transformWidth(int width) {
        return (int) (width / mWidthTransformRatio);
    }

    private int transformHeight(int height) {
        return (int) (height / mHeightTransformRatio);
    }

    @Override
    public void present(ItemLoadedCallback callback) {
        // This is called to show a full-screen slideshow. Presently, all parts of
        // a slideshow (images, sounds, etc.) are loaded and displayed on the UI thread.
        presentSlide((SlideViewInterface) mView, ((SlideshowModel) mModel).get(mLocation));
    }

    /**
     * @param view
     * @param model
     */
    protected void presentSlide(SlideViewInterface view, SlideModel model) {
        view.reset();

        for (MediaModel media : model) {
            if (media instanceof RegionMediaModel) {
                presentRegionMedia(view, (RegionMediaModel) media, true);
            } else if (media.isAudio()) {
                presentAudio(view, (AudioModel) media, true);
            } else if (media.isVcard()) {
                // the media is vcard.
                presentVcard(view, (VcardModel) media, true);
            }
        }
    }

    /**
     * @param view
     */
    protected void presentRegionMedia(SlideViewInterface view,
            RegionMediaModel rMedia, boolean dataChanged) {
        RegionModel r = (rMedia).getRegion();
        if (rMedia.isText()) {
            presentText(view, (TextModel) rMedia, r, dataChanged);
        } else if (rMedia.isImage()) {
            presentImage(view, (ImageModel) rMedia, r, dataChanged);
        } else if (rMedia.isVideo()) {
            presentVideo(view, (VideoModel) rMedia, r, dataChanged);
        }
    }

    protected void presentAudio(SlideViewInterface view, AudioModel audio,
            boolean dataChanged) {
        // Set audio only when data changed.
        if (dataChanged) {
            view.setAudio(audio.getUri(), audio.getSrc(), audio.getExtras());
        }

        MediaAction action = audio.getCurrentAction();
        if (action == MediaAction.START) {
            view.startAudio();
        } else if (action == MediaAction.PAUSE) {
            view.pauseAudio();
        } else if (action == MediaAction.STOP) {
            view.stopAudio();
        } else if (action == MediaAction.SEEK) {
            view.seekAudio(audio.getSeekTo());
        }
    }

    protected void presentVcard(SlideViewInterface view, VcardModel vcard, boolean dataChanged) {
        if (dataChanged) {
            view.setVcard(
                    TextUtils.isEmpty(vcard.getLookupUri()) ? null
                            : Uri.parse(vcard.getLookupUri()), vcard.getSrc());
        }
        if (view instanceof SlideListItemView) {
            SlideListItemView item = (SlideListItemView) view;
            item.setVcard(vcard.getUri(), vcard.getLookupUri(), vcard.getSrc());
        }
    }

    protected void presentText(SlideViewInterface view, TextModel text,
            RegionModel r, boolean dataChanged) {
        if (dataChanged) {
            int text_limit = mContext.getResources().getInteger(R.integer.slide_text_limit_size);
            if ((text_limit > 0) && (text.getText().length() > text_limit)) {
                // It will drop the extra characters in slideview once text
                // exceeds the size of text_limit.
                view.setText(text.getSrc(), text.getText().substring(0, text_limit));
            } else {
                view.setText(text.getSrc(), text.getText());
            }
        }

        if (view instanceof AdaptableSlideViewInterface) {
            ((AdaptableSlideViewInterface) view).setTextRegion(
                    transformWidth(r.getLeft()),
                    transformHeight(r.getTop()),
                    transformWidth(r.getWidth()),
                    transformHeight(r.getHeight()));
        }
        view.setTextVisibility(text.isVisible());
    }

    /**
     * @param view
     * @param image
     * @param r
     */
    protected void presentImage(final SlideViewInterface view, final ImageModel image,
            RegionModel r, boolean dataChanged) {
        final int transformedWidth = transformWidth(r.getWidth());
        final int transformedHeight = transformWidth(r.getHeight());
        final int normalSlideH = (int)((float)transformedHeight * mHeightTransformRatio / 2);
        final int normalSlideW = (int)((float)transformedWidth * mWidthTransformRatio / 2);

        if (LOCAL_LOGV) {
            Log.v(TAG, "presentImage r.getWidth: " + r.getWidth()
                    + ", r.getHeight: " + r.getHeight() +
                    " normalSlideW: " + normalSlideW +
                    " normalSlideH: " + normalSlideH+
                    " transformedWidth: " + transformedWidth +
                    " transformedHeight: " + transformedHeight);
        }

        if (dataChanged) {
            final Handler bitmapHandler = new Handler() {
                @Override
                public void handleMessage(Message message) {
                    if (view instanceof SlideListItemView) {
                            SlideListItemView item = (SlideListItemView) view;
                            item.setUri(image.getUri());
                    }
                    view.setImage(image.getSrc(), (Bitmap)message.obj);
                }
            };
            Thread bitmapLoaderThread = new Thread() {
                @Override
                public void run() {
                    Bitmap drawable;
                    if (view instanceof SlideListItemView) {
                        drawable = image.getBitmap(normalSlideW, normalSlideH);
                    } else {
                        drawable = image.getBitmap(transformedWidth, transformedHeight);
                    }
                    Message message = bitmapHandler.obtainMessage(1, drawable);
                    bitmapHandler.sendMessage(message);
                }
            };
            bitmapLoaderThread.start();
        }

        if (view instanceof AdaptableSlideViewInterface) {
            ((AdaptableSlideViewInterface) view).setImageRegion(
                    transformWidth(r.getLeft()),
                    transformHeight(r.getTop()),
                    transformedWidth,
                    transformedHeight);
        }
        view.setImageRegionFit(r.getFit());
        view.setImageVisibility(image.isVisible());
    }

    /**
     * @param view
     * @param video
     * @param r
     */
    protected void presentVideo(SlideViewInterface view, VideoModel video,
            RegionModel r, boolean dataChanged) {
        if (dataChanged) {
            view.setVideo(video.getSrc(), video.getUri());
        }
        if (view instanceof AdaptableSlideViewInterface) {
            ((AdaptableSlideViewInterface) view).setVideoRegion(
                    transformWidth(r.getLeft()),
                    transformHeight(r.getTop()),
                    transformWidth(r.getWidth()),
                    transformHeight(r.getHeight()));
        } else if (view instanceof SlideListItemView) {
            mItemLoadedFuture = video.loadThumbnailBitmap(mItemLoadedCallback);
        }
        view.setVideoVisibility(video.isVisible());

        MediaAction action = video.getCurrentAction();
        if (action == MediaAction.START) {
            view.startVideo();
        } else if (action == MediaAction.PAUSE) {
            view.pauseVideo();
        } else if (action == MediaAction.STOP) {
            view.stopVideo();
        } else if (action == MediaAction.SEEK) {
            view.seekVideo(video.getSeekTo());
        }
    }

    private ItemLoadedCallback<ImageLoaded> mItemLoadedCallback =
            new ItemLoadedCallback<ImageLoaded>() {
        public void onItemLoaded(ImageLoaded imageLoaded, Throwable exception) {
            if (exception == null) {
                if (mItemLoadedFuture != null) {
                    synchronized(mItemLoadedFuture) {
                        mItemLoadedFuture.setIsDone(true);
                    }
                    mItemLoadedFuture = null;
                }
                SlideModel slide = ((SlideshowModel) mModel).get(mLocation);
                if (slide != null) {
                    if (slide.hasVideo() && imageLoaded.mIsVideo) {
                        ((SlideViewInterface)mView).setVideoThumbnail(null,
                                imageLoaded.mBitmap);
                    }
                }
            }
        }
    };

    public void setLocation(int location) {
        mLocation = location;
    }

    public int getLocation() {
        return mLocation;
    }

    public void goBackward() {
        if (mLocation > 0) {
            mLocation--;
        }
    }

    public void goForward() {
        if (mLocation < (mSlideNumber - 1)) {
            mLocation++;
        }
    }

    public void onModelChanged(final Model model, final boolean dataChanged) {
        final SlideViewInterface view = (SlideViewInterface) mView;
        if (view instanceof SlideView) {
            SlideView v = (SlideView)view;
            mHandler.post(new Runnable() {
                public void run() {
                    v.resetImage();
                }
            });
        }
        // FIXME: Should be optimized.
        if (model instanceof SlideshowModel) {
            // TODO:
        } else if (model instanceof SlideModel) {
            if (((SlideModel) model).isVisible()) {
                mHandler.post(new Runnable() {
                    public void run() {
                        presentSlide(view, (SlideModel) model);
                    }
                });
            } else {
                mHandler.post(new Runnable() {
                    public void run() {
                        goForward();
                    }
                });
            }
        } else if (model instanceof MediaModel) {
            if (model instanceof RegionMediaModel) {
                mHandler.post(new Runnable() {
                    public void run() {
                        presentRegionMedia(view, (RegionMediaModel) model, dataChanged);
                    }
                });
            } else if (((MediaModel) model).isAudio()) {
                mHandler.post(new Runnable() {
                    public void run() {
                        presentAudio(view, (AudioModel) model, dataChanged);
                    }
                });
            }
        } else if (model instanceof RegionModel) {
            // TODO:
        }
    }

    @Override
    public void cancelBackgroundLoading() {
        SlideModel slide = ((SlideshowModel) mModel).get(mLocation);
        if (slide != null && slide.hasVideo()) {
            slide.getVideo().cancelThumbnailLoading();
        }
        mItemLoadedFuture = null;
        mModel.unregisterModelChangedObserver(this);
    }
}

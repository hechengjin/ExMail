/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.Layer.RenderContext;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONObject;

import android.util.Log;
import android.view.View;

class TextSelection extends Layer implements GeckoEventListener {
    private static final String LOGTAG = "GeckoTextSelection";

    private final TextSelectionHandle mStartHandle;
    private final TextSelectionHandle mEndHandle;
    private final EventDispatcher mEventDispatcher;

    private float mViewLeft;
    private float mViewTop;
    private float mViewZoom;

    TextSelection(TextSelectionHandle startHandle, TextSelectionHandle endHandle,
                  EventDispatcher eventDispatcher) {
        mStartHandle = startHandle;
        mEndHandle = endHandle;
        mEventDispatcher = eventDispatcher;

        // Only register listeners if we have valid start/end handles
        if (mStartHandle == null || mEndHandle == null) {
            Log.e(LOGTAG, "Failed to initialize text selection because at least one handle is null");
        } else {
            registerEventListener("TextSelection:ShowHandles");
            registerEventListener("TextSelection:HideHandles");
            registerEventListener("TextSelection:PositionHandles");
        }
    }

    void destroy() {
        unregisterEventListener("TextSelection:ShowHandles");
        unregisterEventListener("TextSelection:HideHandles");
        unregisterEventListener("TextSelection:PositionHandles");
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("TextSelection:ShowHandles")) {
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mStartHandle.setVisibility(View.VISIBLE);
                        mEndHandle.setVisibility(View.VISIBLE);

                        mViewLeft = 0.0f;
                        mViewTop = 0.0f;
                        mViewZoom = 0.0f;
                        LayerView layerView = GeckoApp.mAppContext.getLayerView();
                        if (layerView != null) {
                            layerView.addLayer(TextSelection.this);
                        }
                    }
                });
            } else if (event.equals("TextSelection:HideHandles")) {
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        LayerView layerView = GeckoApp.mAppContext.getLayerView();
                        if (layerView != null) {
                            layerView.removeLayer(TextSelection.this);
                        }

                        mStartHandle.setVisibility(View.GONE);
                        mEndHandle.setVisibility(View.GONE);
                    }
                });
            } else if (event.equals("TextSelection:PositionHandles")) {
                final int startLeft = message.getInt("startLeft");
                final int startTop = message.getInt("startTop");
                final int endLeft = message.getInt("endLeft");
                final int endTop = message.getInt("endTop");

                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mStartHandle.positionFromGecko(startLeft, startTop);
                        mEndHandle.positionFromGecko(endLeft, endTop);
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    @Override
    public void draw(final RenderContext context) {
        // cache the relevant values from the context and bail out if they are the same. we do this
        // because this draw function gets called a lot (once per compositor frame) and we want to
        // avoid doing a lot of extra work in cases where it's not needed.
        if (FloatUtils.fuzzyEquals(mViewLeft, context.viewport.left)
                && FloatUtils.fuzzyEquals(mViewTop, context.viewport.top)
                && FloatUtils.fuzzyEquals(mViewZoom, context.zoomFactor)) {
            return;
        }
        mViewLeft = context.viewport.left;
        mViewTop = context.viewport.top;
        mViewZoom = context.zoomFactor;

        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                mStartHandle.repositionWithViewport(context.viewport.left, context.viewport.top, context.zoomFactor);
                mEndHandle.repositionWithViewport(context.viewport.left, context.viewport.top, context.zoomFactor);
            }
        });
    }

    private void registerEventListener(String event) {
        mEventDispatcher.registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        mEventDispatcher.unregisterEventListener(event, this);
    }
}

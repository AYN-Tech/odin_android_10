/*
 * Copyright (C) 2013 The Android Open Source Project
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

package android.telephonyprovider.cts;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony.Carriers;
import android.test.InstrumentationTestCase;

public class TelephonyProviderTest extends InstrumentationTestCase {
    private ContentResolver mContentResolver;
    private static final String[] APN_PROJECTION = {
        Carriers.TYPE,
        Carriers.MMSC,
        Carriers.MMSPROXY,
        Carriers.MMSPORT,
        Carriers.MVNO_TYPE,
        Carriers.MVNO_MATCH_DATA
    };

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
    }

    // In JB MR1 access to the TelephonyProvider's Carriers table was clamped down and would
    // throw a SecurityException when queried. That was fixed in JB MR2. Verify that 3rd parties
    // can access the APN info the carriers table, after JB MR1.
    public void testAccessToApns() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL";
            String[] selectionArgs = null;
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, null);
        } catch (SecurityException e) {
            fail("No access to current APN");
        }
    }

    public void testNoAccessToPassword() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL AND "
                    + Carriers.PASSWORD + " IS NOT NULL";
            String[] selectionArgs = null;
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, null);
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNoAccessToPasswordThruSort() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL";
            String[] selectionArgs = null;
            String sort = "LIMIT CASE WHEN ((SELECT COUNT(*) FROM carriers WHERE"
                    + " password LIKE 'a%') > 0) THEN 1 ELSE 0 END";
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, sort);
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNoAccessToPasswordThruMixedCase() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL";
            String[] selectionArgs = null;
            String sort = "LIMIT CASE WHEN ((SELECT COUNT(*) FROM carriers WHERE"
                    + " PaSsWoRd LIKE 'a%') > 0) THEN 1 ELSE 0 END";
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, sort);
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNoAccessToUser() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL AND "
                    + Carriers.USER + " IS NOT NULL";
            String[] selectionArgs = null;
            String sort = "LIMIT CASE WHEN ((SELECT COUNT(*) FROM carriers WHERE"
                    + " user LIKE 'a%') > 0) THEN 1 ELSE 0 END";
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, sort);
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNoAccessViaSubqueries() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL";
            String[] selectionArgs = null;
            String sort = "LIMIT CASE WHEN ((SELECT COUNT(*) FROM carriers WHERE"
                    + " mcc LIKE 'a%') > 0) THEN 1 ELSE 0 END";
            Cursor cursor = mContentResolver.query(Carriers.CONTENT_URI,
                    APN_PROJECTION, selection, selectionArgs, sort);
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNoAccessToUserWithDifferentUri() {
        try {
            String selection = Carriers.CURRENT + " IS NOT NULL AND "
                    + Carriers.USER + " IS NOT NULL";
            String[] selectionArgs = null;
            String sort = "LIMIT CASE WHEN ((SELECT COUNT(*) FROM carriers WHERE"
                    + " user LIKE 'a%') > 0) THEN 1 ELSE 0 END";
            Cursor cursor = mContentResolver.query(Uri.parse("content://telephony/siminfo"),
                    APN_PROJECTION, selection, selectionArgs, sort);
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }
}

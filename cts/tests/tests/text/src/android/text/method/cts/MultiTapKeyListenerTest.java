/*
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

package android.text.method.cts;

import com.android.cts.stub.R;

import dalvik.annotation.TestLevel;
import dalvik.annotation.TestTargetClass;
import dalvik.annotation.TestTargetNew;
import dalvik.annotation.ToBeFixed;

import android.app.Activity;
import android.app.Instrumentation;
import android.test.ActivityInstrumentationTestCase2;
import android.text.InputType;
import android.text.Selection;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.method.MultiTapKeyListener;
import android.text.method.TextKeyListener.Capitalize;
import android.view.KeyEvent;
import android.widget.TextView;
import android.widget.TextView.BufferType;

import java.util.concurrent.TimeUnit;

@TestTargetClass(MultiTapKeyListener.class)
public class MultiTapKeyListenerTest extends
        ActivityInstrumentationTestCase2<KeyListenerStubActivity> {
    /**
     * time out of MultiTapKeyListener. longer than 2000ms in case the system is sluggish.
     */
    private static final long TIME_OUT = 3000;
    private Activity mActivity;
    private Instrumentation mInstrumentation;
    private TextView mTextView;

    public MultiTapKeyListenerTest() {
        super("com.android.cts.stub", KeyListenerStubActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = getActivity();
        mInstrumentation = getInstrumentation();
        mTextView = (TextView) mActivity.findViewById(R.id.keylistener_textview);
    }

    @TestTargetNew(
        level = TestLevel.COMPLETE,
        method = "MultiTapKeyListener",
        args = {android.text.method.TextKeyListener.Capitalize.class, boolean.class}
    )
    public void testConstructor() {
        new MultiTapKeyListener(Capitalize.NONE, true);

        new MultiTapKeyListener(Capitalize.WORDS, false);

        new MultiTapKeyListener(null, false);
    }

    @TestTargetNew(
        level = TestLevel.COMPLETE,
        notes = "it is a non-operation method",
        method = "onSpanAdded",
        args = {Spannable.class, Object.class, int.class, int.class}
    )
    public void testOnSpanAdded() {
        final MockMultiTapKeyListener multiTapKeyListener
                = new MockMultiTapKeyListener(Capitalize.CHARACTERS, true);
        final Spannable text = new SpannableStringBuilder("123456");

        assertFalse(multiTapKeyListener.hadAddedSpan());
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                mTextView.setKeyListener(multiTapKeyListener);
                mTextView.setText(text, BufferType.EDITABLE);
            }
        });
        mInstrumentation.waitForIdleSync();

        assertTrue(multiTapKeyListener.hadAddedSpan());
    }

    @TestTargetNew(
        level = TestLevel.PARTIAL,
        method = "onSpanChanged",
        args = {Spannable.class, Object.class, int.class, int.class, int.class, int.class}
    )
    @ToBeFixed(bug = "1695243", explanation = "Android API javadocs are incomplete, " +
            "should add @throws clause into javadoc")
    public void testOnSpanChanged() {
        final MultiTapKeyListener multiTapKeyListener
                = MultiTapKeyListener.getInstance(true, Capitalize.CHARACTERS);
        final Spannable text = new SpannableStringBuilder("123456");
        multiTapKeyListener.onSpanChanged(text, Selection.SELECTION_END, 0, 0, 0, 0);

        try {
            multiTapKeyListener.onSpanChanged(null, Selection.SELECTION_END, 0, 0, 0, 0);
            fail("should throw NullPointerException.");
        } catch (NullPointerException e) {
            // expected.
        }
    }

    public void testOnKeyDown_capitalizeNone() {
        MultiTapKeyListener keyListener = MultiTapKeyListener.getInstance(false, Capitalize.NONE);

        prepareEmptyTextView();

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_4, 2);
        assertEquals("h", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_3, 2);
        assertEquals("he", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_5, 3);
        assertEquals("hel", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_5, 3);
        assertEquals("hell", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_6, 3);
        assertEquals("hello", mTextView.getText().toString());
    }

    public void testOnKeyDown_capitalizeCharacters() {
        MultiTapKeyListener keyListener = MultiTapKeyListener.getInstance(false,
                Capitalize.CHARACTERS);

        prepareEmptyTextView();

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_4, 2);
        assertEquals("H", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_3, 2);
        assertEquals("HE", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_5, 3);
        assertEquals("HEL", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_5, 3);
        assertEquals("HELL", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_6, 3);
        assertEquals("HELLO", mTextView.getText().toString());
    }

    public void testOnKeyDown_capitalizeSentences() {
        MultiTapKeyListener keyListener = MultiTapKeyListener.getInstance(false,
                Capitalize.SENTENCES);

        prepareEmptyTextView();

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_4, 2);
        assertEquals("H", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_4, 3);
        assertEquals("Hi", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_1, 1);
        assertEquals("Hi.", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_POUND, 1);
        assertEquals("Hi. ", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_2, 2);
        assertEquals("Hi. B", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_9, 3);
        assertEquals("Hi. By", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_3, 2);
        assertEquals("Hi. Bye", mTextView.getText().toString());
    }

    public void testOnKeyDown_capitalizeWords() {
        MultiTapKeyListener keyListener = MultiTapKeyListener.getInstance(false,
                Capitalize.WORDS);

        prepareEmptyTextView();

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_4, 2);
        assertEquals("H", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_4, 3);
        assertEquals("Hi", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_POUND, 1);
        assertEquals("Hi ", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_2, 2);
        assertEquals("Hi B", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_9, 3);
        assertEquals("Hi By", mTextView.getText().toString());

        callOnKeyDown(keyListener, KeyEvent.KEYCODE_3, 2);
        assertEquals("Hi Bye", mTextView.getText().toString());
    }

    private void prepareEmptyTextView() {
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                mTextView.setText("", BufferType.EDITABLE);
                Selection.setSelection(mTextView.getEditableText(), 0, 0);
            }
        });
        mInstrumentation.waitForIdleSync();
        assertEquals("", mTextView.getText().toString());
    }

    private void callOnKeyDown(final MultiTapKeyListener keyListener, final int keyCode,
            final int numTimes) {
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                for (int i = 0; i < numTimes; i++) {
                    keyListener.onKeyDown(mTextView, mTextView.getEditableText(), keyCode,
                            new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
                }
            }
        });
        mInstrumentation.waitForIdleSync();

        // Wait a bit in order to distinguish this character and the next one.
        try {
            Thread.sleep(TIME_OUT);
        } catch (InterruptedException e) {
            fail("thrown unexpected InterruptedException when sleep.");
        }
    }

    @TestTargetNew(
        level = TestLevel.COMPLETE,
        method = "getInstance",
        args = {boolean.class, android.text.method.TextKeyListener.Capitalize.class}
    )
    public void testGetInstance() {
        MultiTapKeyListener listener1 = MultiTapKeyListener.getInstance(false, Capitalize.NONE);
        MultiTapKeyListener listener2 = MultiTapKeyListener.getInstance(false, Capitalize.NONE);
        MultiTapKeyListener listener3 = MultiTapKeyListener.getInstance(false, Capitalize.WORDS);
        MultiTapKeyListener listener4 = MultiTapKeyListener.getInstance(true, Capitalize.NONE);

        assertNotNull(listener1);
        assertNotNull(listener2);
        assertSame(listener1, listener2);

        assertNotSame(listener1, listener3);
        assertNotSame(listener4, listener3);
        assertNotSame(listener4, listener1);
    }

    @TestTargetNew(
        level = TestLevel.COMPLETE,
        notes = "it is a non-operation method.",
        method = "onSpanRemoved",
        args = {android.text.Spannable.class, java.lang.Object.class, int.class, int.class}
    )
    public void testOnSpanRemoved() {
        MultiTapKeyListener multiTapKeyListener =
                new MultiTapKeyListener(Capitalize.CHARACTERS, true);
        final Spannable text = new SpannableStringBuilder("123456");
        multiTapKeyListener.onSpanRemoved(text, new Object(), 0, 0);
    }

    @TestTargetNew(
        level = TestLevel.COMPLETE,
        method = "getInputType",
        args = {}
    )
    public void testGetInputType() {
        MultiTapKeyListener listener = MultiTapKeyListener.getInstance(false, Capitalize.NONE);
        int expected = InputType.TYPE_CLASS_TEXT;
        assertEquals(expected, listener.getInputType());

        listener = MultiTapKeyListener.getInstance(true, Capitalize.CHARACTERS);
        expected = InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
                | InputType.TYPE_TEXT_FLAG_AUTO_CORRECT;
        assertEquals(expected, listener.getInputType());
    }

    private class MockMultiTapKeyListener extends MultiTapKeyListener {
        private boolean mHadAddedSpan;

        public MockMultiTapKeyListener(Capitalize cap, boolean autotext) {
            super(cap, autotext);
        }

        @Override
        public void onSpanAdded(Spannable s, Object what, int start, int end) {
            mHadAddedSpan = true;
            super.onSpanAdded(s, what, start, end);
        }

        public boolean hadAddedSpan() {
            return mHadAddedSpan;
        }
    }
}

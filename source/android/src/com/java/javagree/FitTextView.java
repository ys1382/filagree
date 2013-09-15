package com.java.javagree;

import android.content.Context;
import android.graphics.Rect;
import android.text.TextPaint;
import android.util.Log;
import android.widget.TextView;

public class FitTextView extends TextView {

	private final static int FUDGE = 10;

    // Default constructor override
    public FitTextView(Context context) {
        this(context, "");
    }

	public FitTextView(Context context, String text) {
		super(context);
		this.setText(text);
		Rect bounds = new Rect();

		TextPaint paint = getPaint();
        paint.getTextBounds(text, 0, text.length(), bounds);

        int width = bounds.width() + this.getPaddingLeft() + this.getPaddingRight() + FUDGE;
        int height = bounds.height() + this.getPaddingBottom() + this.getPaddingTop() + FUDGE;

        this.setWidth(width);
        this.setHeight(height);
        Log.d("FitTextView", "size " + width +"x"+ height);
	}
}

package com.dethrace.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * Compact GTA SA-style virtual gamepad overlay.
 * <ul>
 *   <li>Bottom-left:  two small round buttons (steer L / steer R)</li>
 *   <li>Bottom-right: two small rounded buttons (brake, gas)</li>
 *   <li>Top-right:    pause/menu button</li>
 * </ul>
 * Touches pass through to the SDL surface underneath.
 */
public class TouchOverlayView extends View {

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint border = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint icon = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint iconStroke = new Paint(Paint.ANTI_ALIAS_FLAG);

    public TouchOverlayView(Context context) {
        super(context);
        init();
    }

    public TouchOverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setWillNotDraw(false);
        setClickable(false);
        setFocusable(false);

        fill.setStyle(Paint.Style.FILL);
        fill.setColor(Color.argb(120, 0, 0, 0));

        border.setStyle(Paint.Style.STROKE);
        border.setStrokeWidth(3.5f);
        border.setColor(Color.argb(220, 240, 240, 240));

        icon.setStyle(Paint.Style.FILL);
        icon.setColor(Color.argb(235, 255, 255, 255));

        iconStroke.setStyle(Paint.Style.STROKE);
        iconStroke.setStrokeWidth(2f);
        iconStroke.setColor(Color.argb(160, 0, 0, 0));
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Pass-through. Touches are handled by SDL on the surface below.
        return false;
    }

    @Override
    protected void onDraw(Canvas c) {
        super.onDraw(c);

        int w = getWidth();
        int h = getHeight();
        if (w == 0 || h == 0) return;

        // Button size scales with the smaller dimension so it looks right on
        // any device. ~9% of height in landscape ≈ 95–110 px.
        float buttonSize = h * 0.18f;
        float gap = buttonSize * 0.20f;
        float margin = h * 0.06f;

        /* ---------- Steering arrows (bottom-left) ---------- */
        float steerCy = h - margin - buttonSize / 2f;
        float steerLeftCx = margin + buttonSize / 2f;
        float steerRightCx = steerLeftCx + buttonSize + gap;
        drawArrowButton(c, steerLeftCx, steerCy, buttonSize / 2f, true);
        drawArrowButton(c, steerRightCx, steerCy, buttonSize / 2f, false);

        /* ---------- Pedals (bottom-right) ---------- */
        float pedalSize = buttonSize * 1.05f;
        float pedalCy = h - margin - pedalSize / 2f;
        float pedalGasCx = w - margin - pedalSize / 2f;
        float pedalBrakeCx = pedalGasCx - pedalSize - gap;
        drawPedalButton(c, pedalBrakeCx, pedalCy, pedalSize / 2f, true);   // brake
        drawPedalButton(c, pedalGasCx, pedalCy, pedalSize / 2f, false);   // gas

        /* ---------- Pause button (top-right) ---------- */
        float pauseSize = buttonSize * 0.75f;
        float pauseCx = w - margin - pauseSize / 2f;
        float pauseCy = margin + pauseSize / 2f;
        drawPauseButton(c, pauseCx, pauseCy, pauseSize / 2f);
    }

    /* ---------- Round arrow button (steer L / R) ---------- */

    private void drawArrowButton(Canvas c, float cx, float cy, float radius, boolean leftArrow) {
        c.drawCircle(cx, cy, radius, fill);
        c.drawCircle(cx, cy, radius, border);

        float a = radius * 0.55f;
        Path tri = new Path();
        if (leftArrow) {
            tri.moveTo(cx - a * 0.55f, cy);
            tri.lineTo(cx + a * 0.45f, cy - a * 0.7f);
            tri.lineTo(cx + a * 0.45f, cy + a * 0.7f);
        } else {
            tri.moveTo(cx + a * 0.55f, cy);
            tri.lineTo(cx - a * 0.45f, cy - a * 0.7f);
            tri.lineTo(cx - a * 0.45f, cy + a * 0.7f);
        }
        tri.close();
        c.drawPath(tri, icon);
        c.drawPath(tri, iconStroke);
    }

    /* ---------- Pedal-style square buttons ---------- */

    private void drawPedalButton(Canvas c, float cx, float cy, float radius, boolean isBrake) {
        float corner = radius * 0.32f;
        RectF rect = new RectF(cx - radius, cy - radius, cx + radius, cy + radius);
        c.drawRoundRect(rect, corner, corner, fill);
        c.drawRoundRect(rect, corner, corner, border);

        if (isBrake) {
            // Up-pointing 'P' style brake symbol — three dots row over a 'P'-ish arrow
            // Simple: large red dot triplet to read as "brake lights"
            Paint dot = new Paint(Paint.ANTI_ALIAS_FLAG);
            dot.setStyle(Paint.Style.FILL);
            dot.setColor(Color.argb(240, 230, 60, 60));
            float dotR = radius * 0.13f;
            float spacing = radius * 0.42f;
            c.drawCircle(cx - spacing, cy - radius * 0.18f, dotR, dot);
            c.drawCircle(cx, cy - radius * 0.18f, dotR, dot);
            c.drawCircle(cx + spacing, cy - radius * 0.18f, dotR, dot);
            c.drawCircle(cx - spacing, cy + radius * 0.30f, dotR, dot);
            c.drawCircle(cx, cy + radius * 0.30f, dotR, dot);
            c.drawCircle(cx + spacing, cy + radius * 0.30f, dotR, dot);
        } else {
            // Gas pedal — grid pattern (like the throttle button in GTA SA)
            Paint grid = new Paint(Paint.ANTI_ALIAS_FLAG);
            grid.setStyle(Paint.Style.FILL);
            grid.setColor(Color.argb(235, 255, 255, 255));
            float gridR = radius * 0.10f;
            float spacing = radius * 0.36f;
            for (int row = -1; row <= 1; row++) {
                for (int col = -1; col <= 1; col++) {
                    c.drawCircle(cx + col * spacing, cy + row * spacing, gridR, grid);
                }
            }
        }
    }

    /* ---------- Pause button (top-right) ---------- */

    private void drawPauseButton(Canvas c, float cx, float cy, float radius) {
        c.drawCircle(cx, cy, radius, fill);
        c.drawCircle(cx, cy, radius, border);

        // Two vertical bars
        float barW = radius * 0.22f;
        float barH = radius * 0.85f;
        float gap = radius * 0.20f;
        Paint bar = new Paint(Paint.ANTI_ALIAS_FLAG);
        bar.setStyle(Paint.Style.FILL);
        bar.setColor(Color.argb(235, 255, 255, 255));
        RectF leftBar = new RectF(cx - gap - barW, cy - barH / 2f, cx - gap, cy + barH / 2f);
        RectF rightBar = new RectF(cx + gap, cy - barH / 2f, cx + gap + barW, cy + barH / 2f);
        c.drawRoundRect(leftBar, barW * 0.4f, barW * 0.4f, bar);
        c.drawRoundRect(rightBar, barW * 0.4f, barW * 0.4f, bar);
    }
}

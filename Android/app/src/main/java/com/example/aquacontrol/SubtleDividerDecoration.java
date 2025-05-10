package com.example.aquacontrol;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.Objects;

public class SubtleDividerDecoration extends RecyclerView.ItemDecoration {
    private final Paint paint;
    private final int height;

    public SubtleDividerDecoration(Context context, int heightDp, int color) {
        this.height = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, heightDp, context.getResources().getDisplayMetrics());

        paint = new Paint();
        paint.setColor(color);
    }

    @Override
    public void onDrawOver(@NonNull Canvas canvas, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
        int childCount = parent.getChildCount();
        int itemCount = Objects.requireNonNull(parent.getAdapter()).getItemCount();

        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(child);

            if (position == RecyclerView.NO_POSITION || position == itemCount - 1) {
                continue;
            }

            float top = child.getBottom();
            float bottom = top + height;

            canvas.drawRect(child.getLeft(), top, child.getRight(), bottom, paint);
        }
    }
}

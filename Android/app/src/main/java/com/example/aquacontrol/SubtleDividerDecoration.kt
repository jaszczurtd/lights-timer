package com.example.aquacontrol

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.TypedValue
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.ItemDecoration
import java.util.Objects
import androidx.core.view.size

class SubtleDividerDecoration(context: Context, heightDp: Int, color: Int) : ItemDecoration() {
    private val paint: Paint
    private val height: Int

    init {
        this.height = TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            heightDp.toFloat(),
            context.getResources().getDisplayMetrics()
        ).toInt()

        paint = Paint()
        paint.setColor(color)
    }

    override fun onDrawOver(canvas: Canvas, parent: RecyclerView, state: RecyclerView.State) {
        val adapter = parent.adapter ?: return
        val itemCount = adapter.itemCount
        val childCount = parent.childCount

        for (i in 0 until childCount) {
            val child = parent.getChildAt(i)
            val position = parent.getChildAdapterPosition(child)
            if (position == RecyclerView.NO_POSITION || position == itemCount - 1) {
                continue
            }

            val top = child.bottom.toFloat()
            val bottom = top + height

            canvas.drawRect(
                child.left.toFloat(),
                top,
                child.right.toFloat(),
                bottom,
                paint
            )
        }
    }
}

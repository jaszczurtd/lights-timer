package com.example.aquacontrol;

import android.app.AlertDialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TimePicker;

import androidx.appcompat.app.AppCompatActivity;

import java.util.Calendar;
import java.util.Locale;

public class TimeRangeDialog extends AppCompatActivity implements Constants{
    public static int getHour(long totalMinutes) {
        return (int)totalMinutes / 60;
    }

    public static int getMinute(long totalMinutes) {
        return (int)totalMinutes % 60;
    }

    public static String formatTime(long time) {
        return String.format(Locale.ENGLISH, "%02d:%02d", getHour(time), getMinute(time));
    }

    public static long getTimeNowInMinutes() {
        Calendar now = Calendar.getInstance();
        int hour = now.get(Calendar.HOUR_OF_DAY);
        int minute = now.get(Calendar.MINUTE);
        return hour * 60 + minute;
    }

    public static boolean isTimeInRange(long now, long start, long end) {
        if (start <= end) {
            return now >= start && now < end;
        } else {
            return now >= start || now < end;
        }
    }

    public interface OnTimeRangeSelected {
        void onRangeSelected(int startMinutes, int endMinutes);
    }

    public static int toMinutes(int hour, int minute) {
        return hour * 60 + minute;
    }

    public static void show(Context context, long start, long end, OnTimeRangeSelected callback) {
        View view = LayoutInflater.from(context).inflate(R.layout.dialog_time_range, null);
        TimePicker timePickerStart = view.findViewById(R.id.timePickerStart);
        TimePicker timePickerEnd = view.findViewById(R.id.timePickerEnd);

        if(start == 0) {
            start = getTimeNowInMinutes();
        }
        if(end == 0) {
            end = getTimeNowInMinutes();
        }

        timePickerStart.setHour(getHour(start));
        timePickerStart.setMinute(getMinute(start));
        timePickerEnd.setHour(getHour(end));
        timePickerEnd.setMinute(getMinute(end));

        timePickerStart.setIs24HourView(true);
        timePickerEnd.setIs24HourView(true);

        AlertDialog dialog = new AlertDialog.Builder(context)
                .setTitle(context.getString(R.string.time_range))
                .setView(view)
                .setPositiveButton(context.getString(R.string.ok), null)
                .setNegativeButton(context.getString(R.string.cancel), null)
                .create();

        dialog.setOnShowListener(d -> {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
                int startHour = timePickerStart.getHour();
                int startMinute = timePickerStart.getMinute();
                int endHour = timePickerEnd.getHour();
                int endMinute = timePickerEnd.getMinute();

                boolean isValid = (endHour > startHour) ||
                        (endHour == startHour && endMinute >= startMinute);

                if (!isValid) {
                    new AlertDialog.Builder(context)
                            .setTitle(context.getString(R.string.error))
                            .setMessage(context.getString(R.string.time_range_error))
                            .setPositiveButton(context.getString(R.string.ok), (dialog1, which) -> dialog1.dismiss())
                            .show();
                } else {
                    dialog.dismiss();
                    callback.onRangeSelected(toMinutes(startHour, startMinute), toMinutes(endHour, endMinute));
                }
            });
        });

        dialog.show();
    }
}

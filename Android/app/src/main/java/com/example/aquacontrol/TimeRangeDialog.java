package com.example.aquacontrol;

import android.app.AlertDialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TimePicker;

import androidx.appcompat.app.AppCompatActivity;

import java.util.Calendar;
import java.util.Locale;

public class TimeRangeDialog extends AppCompatActivity {
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

    public static void show(Context context, OnTimeRangeSelected callback) {
        View view = LayoutInflater.from(context).inflate(R.layout.dialog_time_range, null);
        TimePicker timePickerStart = view.findViewById(R.id.timePickerStart);
        TimePicker timePickerEnd = view.findViewById(R.id.timePickerEnd);

        timePickerStart.setIs24HourView(true);
        timePickerEnd.setIs24HourView(true);

        AlertDialog dialog = new AlertDialog.Builder(context)
                .setTitle("Zakres czasu")
                .setView(view)
                .setPositiveButton("OK", null) // kliknięcie obsługujemy ręcznie niżej
                .setNegativeButton("Anuluj", null)
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
                            .setTitle("Błąd")
                            .setMessage("Czas zakończenia musi być późniejszy niż czas rozpoczęcia!")
                            .setPositiveButton("OK", (dialog1, which) -> dialog1.dismiss())
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

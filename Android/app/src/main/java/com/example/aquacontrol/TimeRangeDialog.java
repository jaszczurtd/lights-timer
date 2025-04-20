package com.example.aquacontrol;

import android.app.AlertDialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TimePicker;

import androidx.appcompat.app.AppCompatActivity;

public class TimeRangeDialog extends AppCompatActivity {
    public interface OnTimeRangeSelected {
        void onRangeSelected(int startHour, int startMinute, int endHour, int endMinute);
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
                    callback.onRangeSelected(startHour, startMinute, endHour, endMinute);
                }
            });
        });

        dialog.show();
    }
}

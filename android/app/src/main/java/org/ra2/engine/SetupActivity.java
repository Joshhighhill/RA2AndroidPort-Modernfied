package org.ra2.engine;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.provider.OpenableColumns;
import android.widget.ImageButton;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * W2-AR-03 / W2-AR-04: the launcher activity. It checks that the user's own
 * Red Alert 2 files are in the app's external files dir
 * (Android/data/org.ra2.engine/files/ra2/, the folder the engine reads) and
 * starts the game when they are. Otherwise it explains what is missing and
 * offers an in-app import: the Storage Access Framework folder picker
 * (ACTION_OPEN_DOCUMENT_TREE -- works on Android 5 to 15, no storage
 * permission) copies the files in under their canonical names. That replaces
 * the old "adb push into Android/data" recipe, which Android 11+ refuses for
 * the shell user and file managers.
 *
 * Runs in its own process (":setup", see the manifest) so the engine's
 * exit() at the end of main() cannot take it down when the engine hands back
 * to it (the missing-files page's IMPORT button).
 */
public class SetupActivity extends Activity {
    private static final String TAG = "RA2Setup";
    private static final int REQ_TREE = 1;
    public static final String EXTRA_IMPORT = "org.ra2.engine.IMPORT";
    /** W6-06-05: the launcher shortcut ("Import game files", res/xml/shortcuts.xml). */
    public static final String ACTION_IMPORT = "org.ra2.engine.action.IMPORT";

    /** The files the engine reads; the first four are required (engine/data_dir.inc). */
    static final String[] FILES = {
        "ra2.mix", "ra2md.mix", "language.mix", "multimd.mix",
        "langmd.mix", "mapsmd03.mix", "theme.mix", "expand01.mix",
        "Maps01.mix", "MAPS02.MIX", "ra2.csf",
    };
    static final int REQUIRED = 4;

    /**
     * W6-06-06: the file a picked document is copied to (relative to the data
     * dir), or null when the engine does not read it. The fixed FILES keep
     * their retail spelling; the patch archives the desktop AssetFS scans
     * (src/asset_fs.cpp: expandNN / ecacheNN / elocalNN), thememd.mix and the
     * movies (MOVIESnn.MIX / MOVMDnn.MIX archives and loose .bik files, which
     * engine/movie_player.inc finds in the data dir and its Movies/ folder,
     * case-insensitively) are copied too.
     */

        private static final int REQ_MAPS = 2;
        private static final int REQ_MODS = 3;
    static boolean isModFile(String name) {
        return name != null && name.toLowerCase(Locale.ROOT).endsWith(".ini");
    }
    static File modsDir(Activity a) {
        File d = new File(dataDir(a), "Mods");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d;
    }
    static File[] modFiles(Activity a) { return modFiles(a, false); }

    static File[] modFiles(Activity a, boolean disabled) {
        final String suffix = disabled ? ".ini.disabled" : ".ini";
        File[] l = modsDir(a).listFiles((d, n) -> n.toLowerCase(Locale.ROOT).endsWith(suffix));
        return l == null ? new File[0] : l;
    }

        private void toggleMods() {
        final File[] on = modFiles(this, false);
        final boolean enabling = on.length == 0;
        final File[] files = enabling ? modFiles(this, true) : on;
        if (files.length == 0) return;
        for (File f : files) {
            final String n = f.getName();
            final File to = new File(f.getParentFile(),
                    enabling ? n.substring(0, n.length() - ".disabled".length()) : n + ".disabled");
            //noinspection ResultOfMethodCallIgnored
            to.delete();
            //noinspection ResultOfMethodCallIgnored
            f.renameTo(to);
        }
        refresh();
        status.setText((enabling ? "Enabled " : "Disabled ") + files.length + " mod file(s).\n\n" + status.getText());
    }

    static boolean isMapFile(String name) {
        if (name == null) return false;
        String l = name.toLowerCase(Locale.ROOT);
        return l.endsWith(".yrm") || l.endsWith(".mpr") || l.endsWith(".map");
    }

    static File mapsDir(Activity a) {
        File d = new File(dataDir(a), "Maps");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d;
    }
    
    static String target(String name) {
        if (name == null) return null;
        String l = name.toLowerCase(Locale.ROOT);
        for (String f : FILES) if (f.toLowerCase(Locale.ROOT).equals(l)) return f;
        if (l.matches("(expand|ecache|elocal)\\d\\d\\.mix")) return l;
        if (l.equals("thememd.mix")) return l;
        if (l.matches("(movies\\d\\d|movmd\\d\\d)\\.mix")) return l;
        if (l.endsWith(".bik") && l.length() > 4) return "Movies/" + l;
        if (l.endsWith(".yrm") || l.endsWith(".mpr") || l.endsWith(".map")) return "Maps/" + name;
        return null;
    }

    private TextView status, title, readiness;
    private ProgressBar progress;
    private Button importButton, mapsButton, modsButton, clearModsButton, startButton, exitButton;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean copying = false;
    // R-161: the copy thread is owned by the activity; leaving (finish /
    // destroy) cancels it, and its partial file is deleted.
    private Thread importThread;
    private volatile boolean cancelImport = false;

    static File dataDir(Activity a) {
        File ext = a.getExternalFilesDir(null);
        File d = new File(ext != null ? ext : a.getFilesDir(), "ra2");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();   // visible over MTP even before the first import
        return d;
    }

    static List<String> missing(File dir, boolean requiredOnly) {
        List<String> m = new ArrayList<>();
        for (int i = 0; i < (requiredOnly ? REQUIRED : FILES.length); ++i) {
            File f = new File(dir, FILES[i]);
            if (!f.isFile() || f.length() == 0) m.add(FILES[i]);
        }
        return m;
    }

    /** R-161: delete *.part leftovers of an interrupted copy (app killed mid-import). */
    static int cleanParts(File dir) {
        int n = 0;
        File[] list = dir.listFiles();
        if (list == null) return 0;
        for (File f : list) {
            if (f.isFile() && f.getName().endsWith(".part") && f.delete()) ++n;
            else if ((f.getName().equals("Movies") || f.getName().equals("Maps"))) n += cleanParts(f);
        }
        if (n > 0) Log.i(TAG, "removed " + n + " partial file(s) of an interrupted import");
        return n;
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        File dir = dataDir(this);
        cleanParts(dir);
        mapsDir(this);   // Maps/ always exists, so users can also drop files in by hand
        modsDir(this);
        // Keep the launcher reachable after a crash so reports can be shared.
        buildUi();
        refresh();
    }

    private int dp(int v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private Button button(String label, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setMinHeight(dp(56));   // >= 48 dp touch target
        b.setTextColor(Color.rgb(225, 234, 240));
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        b.setAllCaps(false);
        b.setBackground(panel(Color.rgb(28, 42, 52), Color.rgb(61, 80, 92)));
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(6), dp(6), dp(6), dp(6));
        b.setLayoutParams(lp);
        return b;
    }

    private GradientDrawable panel(int fill, int edge) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(fill);
        d.setCornerRadius(dp(12));
        d.setStroke(dp(1), edge);
        return d;
    }

    private TextView label(String text, int size, int color) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        t.setTextColor(color);
        t.setPadding(dp(8), dp(6), dp(8), dp(6));
        return t;
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.rgb(12, 20, 27));
        root.setPadding(dp(16), dp(12), dp(16), dp(12));
        ScrollView page = new ScrollView(this);
        page.setFillViewport(true);
        page.addView(root);

        title = label(getString(R.string.setup_title), 25, Color.rgb(255, 200, 90));
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
                LinearLayout head = new LinearLayout(this);
        head.setGravity(Gravity.CENTER_VERTICAL);
        title.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        head.addView(title);
        ImageButton rep = new ImageButton(this);
        rep.setImageResource(android.R.drawable.ic_menu_share);
        rep.setColorFilter(Color.rgb(135, 163, 181));
        rep.setBackground(panel(Color.rgb(28, 42, 52), Color.rgb(61, 80, 92)));
        rep.setContentDescription("Share game report");
        rep.setOnClickListener(v -> Diagnostics.share(this));
        head.addView(rep, new LinearLayout.LayoutParams(dp(48), dp(48)));
        root.addView(head);
        root.addView(label("COMMAND & CONQUER  /  ANDROID", 11, Color.rgb(135, 163, 181)));
        readiness = label("Checking game files...", 15, Color.rgb(140, 225, 185));
        readiness.setBackground(panel(Color.rgb(20, 39, 43), Color.rgb(45, 92, 86)));
        readiness.setPadding(dp(16), dp(12), dp(16), dp(12));
        root.addView(readiness);

        startButton = button(getString(R.string.setup_start), v -> startGame());
        startButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 19);
        startButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        startButton.setBackground(panel(Color.rgb(36, 106, 99), Color.rgb(73, 169, 148)));
        LinearLayout launch = new LinearLayout(this);
        launch.addView(startButton);
        root.addView(launch);
        root.addView(label("GAME FILES & DIAGNOSTICS", 12, Color.rgb(135, 163, 181)));
        status = label("", 14, Color.rgb(220, 230, 237));
        status.setPadding(dp(16), dp(12), dp(16), dp(12));
        status.setBackground(panel(Color.rgb(20, 31, 41), Color.rgb(44, 61, 73)));
        root.addView(status);

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        root.addView(progress);

        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER);
        importButton = button(getString(R.string.setup_import), v -> pickFolder());
        exitButton = button(getString(R.string.setup_exit), v -> finish());
        row.addView(importButton);
        mapsButton = button("Import maps", v -> pickMaps());
        row.addView(mapsButton);
        root.addView(row);
        LinearLayout modRow = new LinearLayout(this);
        modsButton = button("Import mods", v -> pickMods());
        clearModsButton = button("Disable mods", v -> toggleMods());
        modRow.addView(modsButton);
        modRow.addView(clearModsButton);
        root.addView(modRow);
        LinearLayout footer = new LinearLayout(this);
        footer.addView(button("Refresh file status", v -> { if (!copying) refresh(); }));
        footer.addView(exitButton);
        root.addView(footer);
        root.addView(label("Uses your own PC game files. Display and touch options are available in the game's Options menu.", 12, Color.rgb(135, 163, 181)));
        setContentView(page);
    }

    private void refresh() {
        File dir = dataDir(this);
        List<String> req = missing(dir, true);
        List<String> all = missing(dir, false);
        StringBuilder sb = new StringBuilder();
        sb.append(getString(R.string.setup_intro)).append("\n\n");
        sb.append("After a crash, tap the share icon (top right) before starting another game.\n\n");
        sb.append("Custom maps: tap Import maps and pick .yrm / .mpr / .map files.\n\n");
        sb.append(getString(R.string.setup_folder)).append("\n  ").append(dir.getAbsolutePath()).append("\n\n");
        // W6-06-05: opened to add / replace files with everything present.
        title.setText(req.isEmpty() ? R.string.setup_title_ready : R.string.setup_title);
        readiness.setText(copying ? "IMPORTING GAME FILES..." :
                (req.isEmpty() ? "READY TO PLAY - Yuri's Revenge" :
                 "SETUP REQUIRED - " + req.size() + " required files missing"));
        if (req.isEmpty()) {
            sb.append(getString(R.string.setup_ready)).append("\n");
        } else {
            sb.append(getString(R.string.setup_missing_required)).append("\n");
            for (String f : req) sb.append("  ").append(f).append("\n");
        }
        List<String> opt = new ArrayList<>(all);
        opt.removeAll(req);
        if (!opt.isEmpty()) {
            sb.append("\n").append(getString(R.string.setup_missing_optional)).append("\n");
            for (String f : opt) sb.append("  ").append(f).append("\n");
        }
        sb.append("\n").append(getString(R.string.setup_how));
        sb.append("\n\n").append(getString(R.string.setup_reimport_note));   // W6-06-05
        status.setText(sb.toString());
        startButton.setEnabled(req.isEmpty() && !copying);
        importButton.setEnabled(!copying);
        mapsButton.setEnabled(!copying);
        modsButton.setEnabled(!copying);
        final int nOn = modFiles(this).length, nOff = modFiles(this, true).length;
        clearModsButton.setText(nOn > 0 ? "Disable mods (" + nOn + ")" : nOff > 0 ? "Enable mods (" + nOff + ")" : "No mods");
        clearModsButton.setEnabled(!copying && (nOn > 0 || nOff > 0));
        exitButton.setEnabled(!copying);   // R-161: no Exit in the middle of a copy
    }

    /** R-161: Back does not abandon a running copy either. */
    @Override
    public void onBackPressed() {
        if (copying) return;
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        if (importThread != null && importThread.isAlive()) {
            cancelImport = true;   // the thread deletes its .part file and stops
            try { importThread.join(2000); } catch (InterruptedException ignored) { }
        }
        super.onDestroy();
    }

    private void pickFolder() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        // R-160: open the picker at Download/RA2 (where the instructions say
        // to put the files). Android 11+ refuses Download itself as a tree,
        // so the user must open the RA2 sub-folder and tap USE THIS FOLDER.
        if (Build.VERSION.SDK_INT >= 26) {
            i.putExtra(DocumentsContract.EXTRA_INITIAL_URI, DocumentsContract.buildDocumentUri(
                    "com.android.externalstorage.documents", "primary:Download/RA2"));
        }
        startActivityForResult(i, REQ_TREE);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
                if (req == REQ_MAPS || req == REQ_MODS) { if (res == RESULT_OK && data != null) importFilesFrom(data, req == REQ_MODS); return; }
        if (req != REQ_TREE || res != RESULT_OK || data == null || data.getData() == null) return;
        final Uri tree = data.getData();
        copying = true;
        cancelImport = false;
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        refresh();
        importThread = new Thread(() -> importFrom(tree), "ra2-import");
        importThread.start();
    }

    /**
     * Lists the game files directly inside `docId` (case-insensitive, see
     * target()); sub-folders go to `subdirs` (document id, display name).
     */
    private void scan(ContentResolver cr, Uri tree, String docId,
                      Map<String, Uri> found, Map<String, Long> sizes, List<String[]> subdirs) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        try (Cursor c = cr.query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String name = c.getString(1);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(3))) {
                    if (subdirs != null) subdirs.add(new String[]{c.getString(0), name == null ? "" : name});
                    continue;
                }
                String key = target(name);
                if (key == null || found.containsKey(key)) continue;
                found.put(key, DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0)));
                sizes.put(key, c.isNull(2) ? 0L : c.getLong(2));
            }
        }
    }

    /** Copies every known file found in the picked folder (case-insensitive match). */
    private void importFrom(Uri tree) {
        ContentResolver cr = getContentResolver();
        File dir = dataDir(this);
        Map<String, Uri> found = new HashMap<>();
        Map<String, Long> sizes = new HashMap<>();
        long total = 0;
        String error = null;
        try {
            // R-160: the picked folder itself, then (when it holds none of the
            // required files) one level of sub-folders -- picking the folder
            // that CONTAINS RA2 (e.g. the copied RA2-phone package) works too.
            List<String[]> subdirs = new ArrayList<>();
            scan(cr, tree, DocumentsContract.getTreeDocumentId(tree), found, sizes, subdirs);
            boolean anyRequired = false;
            for (int i = 0; i < REQUIRED; ++i) anyRequired |= found.containsKey(FILES[i]);
            List<String[]> deeper = new ArrayList<>();
            for (String[] sub : subdirs) {
                // W6-06-06: a Movies folder next to the files is always read;
                // R-160: other sub-folders only when the picked folder holds
                // none of the required files (the folder that CONTAINS RA2).
                if (!anyRequired || sub[1].equalsIgnoreCase("movies") || sub[1].equalsIgnoreCase("maps"))
                    scan(cr, tree, sub[0], found, sizes, anyRequired ? null : deeper);
            }
            for (String[] sub : deeper)
                if (sub[1].equalsIgnoreCase("movies") || sub[1].equalsIgnoreCase("maps")) scan(cr, tree, sub[0], found, sizes, null);
            if (found.isEmpty()) throw new java.io.IOException(getString(R.string.setup_no_files));
            for (Long sz : sizes.values()) total += sz;
            long done = 0;
            byte[] buf = new byte[1 << 20];
            for (Map.Entry<String, Uri> e : found.entrySet()) {
                if (cancelImport) throw new java.io.IOException("cancelled");
                File out = new File(dir, e.getKey());
                File tmp = new File(dir, e.getKey() + ".part");
                File parent = out.getParentFile();
                //noinspection ResultOfMethodCallIgnored
                if (parent != null) parent.mkdirs();   // Movies/
                final String name = e.getKey();
                ui.post(() -> status.setText(getString(R.string.setup_copying, name)));
                try (InputStream in = cr.openInputStream(e.getValue());
                     OutputStream os = new FileOutputStream(tmp)) {
                    if (in == null) throw new java.io.IOException("cannot open " + name);
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        if (cancelImport) throw new java.io.IOException("cancelled");
                        os.write(buf, 0, n);
                        done += n;
                        final int p = total > 0 ? (int) (done * 1000 / total) : 0;
                        ui.post(() -> progress.setProgress(p));
                    }
                }
                Long want = sizes.get(name);
                if (want != null && want > 0 && tmp.length() != want)
                    throw new java.io.IOException(name + ": copied " + tmp.length() + " of " + want + " bytes");
                //noinspection ResultOfMethodCallIgnored
                out.delete();
                if (!tmp.renameTo(out)) throw new java.io.IOException("cannot rename " + tmp);
                Log.i(TAG, "imported " + name + " (" + out.length() + " bytes)");
            }
            Log.i(TAG, "import done: " + found.size() + " files, " + total + " bytes");
        } catch (Exception ex) {
            Log.e(TAG, "import failed", ex);
            error = ex.getMessage();
            cleanParts(dir);   // R-161: never leave a partial file behind
        }
        if (cancelImport) return;   // the activity is gone
        final String err = error;
        final int count = found.size();
        ui.post(() -> {
            copying = false;
            progress.setVisibility(View.GONE);
            refresh();
            String head = err != null ? getString(R.string.setup_failed, err)
                                      : getString(R.string.setup_imported, count);
            status.setText(head + "\n\n" + status.getText());
            if (err == null && missing(dataDir(this), true).isEmpty()) startGame();
        });
    }

        private void pickMaps() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");   // .yrm/.mpr have no MIME type; filtered by extension below
        i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        startActivityForResult(i, REQ_MAPS);
    }
    
    private void pickMods() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        startActivityForResult(i, REQ_MODS);
    }

        private void importFilesFrom(Intent data, final boolean mods) {
        final List<Uri> uris = new ArrayList<>();
        if (data.getClipData() != null)
            for (int k = 0; k < data.getClipData().getItemCount(); ++k)
                uris.add(data.getClipData().getItemAt(k).getUri());
        else if (data.getData() != null) uris.add(data.getData());
        if (uris.isEmpty()) return;
        copying = true;
        refresh();
        new Thread(() -> {
            ContentResolver cr = getContentResolver();
            File maps = mods ? modsDir(this) : mapsDir(this);
            int ok = 0, skipped = 0;
            for (Uri u : uris) {
                String name = null;
                try (Cursor c = cr.query(u, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                    if (c != null && c.moveToFirst()) name = c.getString(0);
                } catch (Exception ignored) { }
                if (!(mods ? isModFile(name) : isMapFile(name))) { ++skipped; continue; }
                name = name.replaceAll("[\\\\/:*?\"<>|]", "_").replaceAll("^\\.+", "");
                if (mods) name = name.toLowerCase(Locale.ROOT);
                File out = new File(maps, name), tmp = new File(maps, name + ".part");
                try (InputStream in = cr.openInputStream(u); OutputStream os = new FileOutputStream(tmp)) {
                    if (in == null) throw new java.io.IOException("cannot open");
                    byte[] buf = new byte[64 * 1024];
                    int n;
                    while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                } catch (Exception ex) {
                    Log.e(TAG, "map import failed: " + name, ex);
                    //noinspection ResultOfMethodCallIgnored
                    tmp.delete(); ++skipped; continue;
                }
                //noinspection ResultOfMethodCallIgnored
                out.delete();
                                if (tmp.renameTo(out)) {
                    ++ok;
                    if (mods) new File(maps, name + ".disabled").delete();   // a fresh import replaces an old disabled copy
                } else ++skipped;
            }
               final String msg = "Imported " + ok + (mods ? " mod file(s)" : " map(s)") + (skipped > 0 ? ", skipped " + skipped : "") + ".";
            ui.post(() -> {
                copying = false;
                refresh();
                status.setText(msg + "\n\n" + status.getText());
            });
        }, "ra2-map-import").start();
    }

    private void startGame() {
        // NEW_TASK only: RA2Activity is singleInstance, so a running game
        // (backgrounded with Home) is brought back as it was, not restarted.
        Intent i = new Intent(this, RA2Activity.class);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
        finish();
    }
        }

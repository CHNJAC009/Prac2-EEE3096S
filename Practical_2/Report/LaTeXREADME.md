# Prac 2 report: LaTeX in VS Code

The report builds on your own laptop in VS Code and is shared through this Git repo.

## One-time setup (each of us)

1. Install **MiKTeX**: `winget install MiKTeX.MiKTeX`. Then open **MiKTeX Console**, click
   **Check for updates** and install them, and under **Settings** set
   *"Always install missing packages on-the-fly"*.
2. Install **Strawberry Perl** (`latexmk` needs it): `winget install StrawberryPerl.StrawberryPerl`
3. In VS Code, install the extension **LaTeX Workshop** (by James Yu).
4. Restart VS Code.

The first build downloads a lot of LaTeX packages and can take a few minutes. Later builds take seconds.

If `winget` isn't available, download the installers instead: MiKTeX from <https://miktex.org/download> and
Strawberry Perl from <https://strawberryperl.com>.

### First time on a new laptop (partner checklist)

1. Get the repo:
   - first time: `git clone https://github.com/CHNJAC009/Prac2-EEE3096S.git`
   - already have it: `git pull`

   The build settings (`.vscode/settings.json` and `Practical_2/Report/.latexmkrc`) come with the repo, so
   there is nothing to configure.
2. Do steps 1–4 above.
3. In VS Code, **File → Open Folder…** and choose the `Prac2-EEE3096S` folder (the repo root).
4. Open `Practical_2/Report/main.tex` and press **Ctrl+S**. If MiKTeX asks to install packages, click
   **Install**.
5. Press **Ctrl+Alt+V**. You should see the report with the title page and our names. If you do, you're set up.
   If not, see *When a build fails* below.

## Everyday use

1. `git pull` so you have your partner's latest changes.
2. In VS Code, **File → Open Folder…** and open the **repo root** (`Prac2-EEE3096S`), not the `Report` folder.
   The build settings live in the repo's `.vscode/settings.json`.
3. Open `Practical_2/Report/main.tex` or any file in `sections/`, edit, and **save (Ctrl+S)**. It builds on
   every save.
4. To see the PDF, click the **TeX** icon in the left sidebar, then **View LaTeX PDF**, or press
   **Ctrl+Alt+V**. It opens in a tab and refreshes after each build.
   **Ctrl+click** in the PDF jumps to the matching line in the source.
5. Commit and push **only the source files** (`.tex`, `.bib`, `figures/`). Everything in `build/` is ignored.

## Where things go

| File | What it holds |
|---|---|
| `main.tex` | Packages, title page, section order. You rarely need to touch it. |
| `values.tex` | **Numbers used in several places**: names, n1/n2, B, A, SCK. Change a value here once and it updates everywhere. |
| `sections/01_gpio.tex` … `06_fsm.tex` | One file per report section, and one task each. |
| `figures/` | Screenshots. The file names each section expects are in `figures/README.md`. |
| `references.bib` | The documents we cite. |
| `build/main.pdf` | The compiled report (generated, not committed). |

## The red placeholders

- `\fillin{...}` prints **red bold text in brackets**, for a value still to add. Replace the whole
  `\fillin{...}` with the value.
- `\writeup{...}` prints a **red box with a prompt**, for a paragraph still to write. Replace the whole
  `\writeup{...}` with your paragraph.
- A missing screenshot shows a **red "Insert figures/…" box**. Save the image with that exact name and the
  box turns into the picture.
- To see what's left, press **Ctrl+Shift+F** and search for `fillin` or `writeup`.

## LaTeX quick reference

| You want | Write |
|---|---|
| Code, register or variable name | `\reg{SPI2\_CR1}` (an underscore must be written `\_`) |
| Cite a document | `\cite[\S28.9.1]{rm0091}` · `\cite[Tab.~15]{stm32f051ds}` · `\cite[p.~5]{eeprom}` |
| Refer to a table/figure/equation | `\cref{tab:pins}` gives "Table 3" (use the `\label{...}` name) |
| A value from `values.tex` | Just the command, e.g. `\BHex`. **Never** type `\n9` or `\0x44`: a backslash starts a command name. |
| Special characters | `\%` `\&` `\_` `\#` · a non-breaking space: `~` (e.g. `5~ms`) |
| New paragraph | Leave a blank line |

## When a build fails

- VS Code shows the error in the **Problems** panel (Ctrl+Shift+M), with the file and line number. The
  mistake is usually on that line or the line just above it.
- Common causes:
  - a missing `}`;
  - an unescaped `_`, `%`, `&` or `#`;
  - a misspelt command such as `\BHEX` instead of `\BHex`.
- If the build gets stuck after a fixed error: **TeX sidebar → Clean up auxiliary files**, then save
  again. From a terminal in `Practical_2/Report/`, `latexmk -C` does the same.
- *"You have not checked for MiKTeX updates"* is only a reminder: run the update in MiKTeX Console.

## Working together without conflicts

- **Pull before you start, and commit and push when you stop.** Small, frequent commits are easiest.
- **Split the work by section file.** Two people editing *different* files never conflict. Agree who owns which
  `sections/0X_*.tex` before you start.
- If you both change `values.tex`, change different lines. Git merges that without trouble.

## Submitting

1. Make sure a search for `fillin` and `writeup` finds nothing.
2. Check the length: **at most 8 pages, not counting the title page and references.**
3. Copy `build/main.pdf` to **`prac_02_SBYSIB014_CHNJAC009.pdf`** and upload that to Gradescope.

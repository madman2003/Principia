/*
 * Copyrightę (c) 2018 Maarten Maathuis, (aka madman2003).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
 
using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using UnityEngine;

namespace principia {
namespace ksp_plugin_adapter {

    [KSPAddon(KSPAddon.Startup.Flight, false)]
    public sealed class PlannerGUI: MonoBehaviour
    {
        private const float button_width = 100.0f;
        private const float button_height = 25.0f;
        private const string magic_maneuver_string = "Delete me please";

        private bool planner_window_visible = false;

        private DialogGUIVerticalLayout planner_page;
        private MultiOptionDialog multi_page_planner;
        private PopupDialog popup_dialog;

        private KSP.UI.Screens.ApplicationLauncherButton toolbar_button;

        public PlannerGUI()
        {
            InitializePlannerGUI();

            GameEvents.onGUIApplicationLauncherReady.Add(onAppLauncherReady);
            GameEvents.onGUIApplicationLauncherDestroyed.Add(onAppLauncherDestroyed);
        }

        private void ForceGUIUpdate(DialogGUIBase parent, DialogGUIBase child)
        {
            Stack<Transform> stack = new Stack<Transform>(); // some data on hierarchy of the GUI components
            stack.Push(parent.uiItem.gameObject.transform); // need the reference point of the parent GUI component for position and size
            child.Create(ref stack, HighLogic.UISkin); // required to force the GUI creation
        }

        private void OnButtonClick_AddManeuver()
        {
            List<DialogGUIBase> rows = planner_page.children;
            DialogGUIVerticalLayout maneuver = new DialogGUIVerticalLayout(false, true, 0, new RectOffset(), TextAnchor.UpperCenter,
                new DialogGUILabel("testing 102")
            );
            maneuver.SetOptionText(magic_maneuver_string);
            rows.Add(maneuver);
            ForceGUIUpdate(planner_page, maneuver);
        }

        private void OnButtonClick_DeleteManeuver()
        {
            List<DialogGUIBase> rows = planner_page.children;
            DialogGUIBase child = rows.ElementAt(rows.Count - 1);
            if (child.OptionText == magic_maneuver_string) {
                rows.RemoveAt(rows.Count - 1);
                child.uiItem.gameObject.DestroyGameObjectImmediate(); // ensure memory gets freed
            }
        }

        private void InitializePlannerGUI()
        {
            planner_page = new DialogGUIVerticalLayout(false, true, 0, new RectOffset(), TextAnchor.UpperCenter,
                new DialogGUIHorizontalLayout(true, false, 0, new RectOffset(), TextAnchor.MiddleCenter,
                    new DialogGUIButton("Add Maneuver", OnButtonClick_AddManeuver, button_width, button_height, false),
                    new DialogGUIButton("Delete Maneuver", OnButtonClick_DeleteManeuver, button_width, button_height, false)
                )
            );

            multi_page_planner = new MultiOptionDialog(
                "PrincipiaPlannerGUI",
                "",
                "Principia Planner",
                HighLogic.UISkin,
                new Rect(0.5f, 0.5f, 450.0f, 50.0f),
                new DialogGUIBase[]
                {
                    // page that contains the actual content
                    planner_page
                });
        }

        // Returns false and nulls |texture| if the file does not exist.
        private bool LoadTextureIfExists(out UnityEngine.Texture texture,
                                         String path) {
            string full_path =
                KSPUtil.ApplicationRootPath + Path.DirectorySeparatorChar +
                "GameData" + Path.DirectorySeparatorChar +
                "Principia" + Path.DirectorySeparatorChar +
                "assets" + Path.DirectorySeparatorChar +
                path;
            if (File.Exists(full_path)) {
                var texture2d = new UnityEngine.Texture2D(2, 2);
#if KSP_VERSION_1_5_1
                bool success = UnityEngine.ImageConversion.LoadImage(
                    texture2d, File.ReadAllBytes(full_path));
#elif KSP_VERSION_1_3_1
                bool success = texture2d.LoadImage(
                    File.ReadAllBytes(full_path));
#endif
                if (!success) {
                    Log.Fatal("Failed to load texture " + full_path);
                }
                texture = texture2d;
                return true;
            } else {
                texture = null;
                return false;
            }
        }
                
        private UnityEngine.Texture LoadTextureOrDie(String path) {
            UnityEngine.Texture texture;
            bool success = LoadTextureIfExists(out texture, path);
            if (!success) {
                Log.Fatal("Missing texture " + path);
            }
            return texture;
        }

        private void ShowPlannerWindow()
        {
            planner_window_visible = true;
            popup_dialog = PopupDialog.SpawnPopupDialog(new Vector2(0.5f, 0.5f), new Vector2(0.5f, 0.5f), 
                                                        multi_page_planner, true, HighLogic.UISkin, false);
        }

        private void HidePlannerWindow()
        {
            planner_window_visible = false;
            if (popup_dialog) {
                popup_dialog.Dismiss();
            }
        }        

        private void InitializeToolbarIcon()
        {
            if (toolbar_button == null) {
                UnityEngine.Texture toolbar_button_texture = LoadTextureOrDie("toolbar_button.png");
                toolbar_button = KSP.UI.Screens.ApplicationLauncher.Instance.AddModApplication(
                      onTrue          : ShowPlannerWindow,
                      onFalse         : HidePlannerWindow,
                      onHover         : null,
                      onHoverOut      : null,
                      onEnable        : null,
                      onDisable       : null,
                      visibleInScenes : KSP.UI.Screens.ApplicationLauncher.AppScenes.FLIGHT,
                      texture         : toolbar_button_texture);
            }
        }
        private void onAppLauncherReady() {
            InitializeToolbarIcon();
        }
        
        private void onAppLauncherDestroyed() {
            KSP.UI.Screens.ApplicationLauncher.Instance.RemoveModApplication(toolbar_button);
            toolbar_button = null;
        }

        public void Start()
        {
            if (planner_window_visible) {
                ShowPlannerWindow();
            }
        }
    }
}  // namespace ksp_plugin_adapter
}  // namespace principia
#include <pebble.h>

#ifdef PBL_SDK_2
#include "action_menu.h"

#define ACTION_MENU_FONT_SMALL  FONT_KEY_GOTHIC_18_BOLD
#define ACTION_MENU_FONT_NORMAL FONT_KEY_GOTHIC_24_BOLD
#define ACTION_MENU_FONT_BIG    FONT_KEY_GOTHIC_28_BOLD

// Choose you favourite font size
#define ACTION_MENU_FONT ACTION_MENU_FONT_NORMAL

struct ActionMenuItem {
  char *label;
  void *action_data;
  ActionMenuPerformActionCb cb;

  const ActionMenuLevel *child;
};

struct ActionMenuLevel {
  uint16_t         max_items;
  uint16_t         num_items;
  ActionMenuItem** items;
  ActionMenuLevelDisplayMode display_mode;

  uint16_t        level;
  const ActionMenuLevel *parent;
};

struct ActionMenu {
  const ActionMenuLevel *current_level;
  const ActionMenuLevel *tmp_level;
  const ActionMenuItem  *performed_action;

  ActionMenuConfig *config;
  Window        *result_window;
  bool          frozen;

  Window        *window;
  Layer         *bg_layer;
  Layer         *column_layer;
  MenuLayer     *menulayer;
  GBitmap       *arrow_image;
  PropertyAnimation *prop_animation;
};

static const uint8_t ARROW_IMAGE_DATA[] = {0x04, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x05, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00};

#define MENU_LAYER_OFFSET 14

//! Getter for the label of a given \ref ActionMenuItem
//! @param item the \ref ActionMenuItem of interest
//! @return a pointer to the string label. NULL if invalid.
char *action_menu_item_get_label(const ActionMenuItem *item) {
  return item ? item->label : NULL;
}

//! Getter for the action_data pointer of a given \ref ActionMenuitem.
//! @see action_menu_level_add_action
//! @param item the \ref ActionMenuItem of interest
//! @return a pointer to the data. NULL if invalid.
void *action_menu_item_get_action_data(const ActionMenuItem *item){
  return item ? item->action_data : NULL;
}

//! Create a new action menu level with storage allocated for a given number of items
//! @param num_items the number of items that will be displayed at that level
//! @note levels are freed alongside the whole hierarchy so no destroy API is provided.
//! @note by default, levels are using ActionMenuLevelDisplayModeWide.
//! Use \ref action_menu_level_set_display_mode to change it.
//! @see action_menu_hierarchy_destroy
ActionMenuLevel *action_menu_level_create(uint16_t num_items){
  ActionMenuLevel* level = malloc(sizeof(ActionMenuLevel));
  if(level) {
    memset(level, 0, sizeof(ActionMenuLevel));
    level->display_mode = ActionMenuLevelDisplayModeWide;
    level->num_items = 0;
    level->max_items = num_items;
    level->level = 1;
    level->items = malloc(num_items * sizeof(ActionMenuItem*));
    if(level->items == NULL){
      free(level);
      level = NULL;
    }
    else {
      memset(level->items, 0, num_items * sizeof(ActionMenuItem*));
    }

  }
  return level;
}

//! Set the action menu display mode
//! @param level The ActionMenuLevel whose display mode you want to change
//! @param display_mode The display mode for the action menu (3 vs. 1 item per row)
void action_menu_level_set_display_mode(ActionMenuLevel *level,
                                        ActionMenuLevelDisplayMode display_mode){
  if(level) {
    level->display_mode = display_mode;
  }
}

//! Add an action to an ActionLevel
//! @param level the level to add the action to
//! @param label the text to display for the action in the menu
//! @param cb the callback that will be triggered when this action is actuated
//! @param action_data data to pass to the callback for this action
//! @return a reference to the new \ref ActionMenuItem on success, NULL if the level is full
ActionMenuItem *action_menu_level_add_action(ActionMenuLevel *level,
                                             const char *label,
                                             ActionMenuPerformActionCb cb,
                                             void *action_data){
  ActionMenuItem* item = NULL;
  if(level && level->num_items < level->max_items) {
    item = malloc(sizeof(ActionMenuItem));
    if(item) {
      memset(item, 0, sizeof(ActionMenuItem));
      if(label){
        item->label = malloc(strlen(label) + 1); 
        if(item->label == NULL) {
          free(item);
          item = NULL;
          return item;
        }
        strcpy(item->label, label);
      }
      item->cb = cb;
      item->action_data = action_data;
      level->items[level->num_items] = item;
      level->num_items = level->num_items+1;
    }
  }
  return item;
}

//! Add a child to this ActionMenuLevel
//! @param level the parent level
//! @param child the child level
//! @param label the text to display in the action menu for this level
//! @return a reference to the new \ref ActionMenuItem on success, NULL if the level is full
ActionMenuItem *action_menu_level_add_child(ActionMenuLevel *level,
                                            ActionMenuLevel *child,
                                            const char *label){
  ActionMenuItem* item = NULL;
  if(level && level->num_items < level->max_items) {
    item = malloc(sizeof(ActionMenuItem));
    if(item) {
      memset(item, 0, sizeof(ActionMenuItem));
      item->child = child;
      child->parent = level;
      if(label){
        item->label = malloc(strlen(label) + 1); 
        if(item->label == NULL) {
          free(item);
          item = NULL;
          return item;
        }
        strcpy(item->label, label);
      }
      child->level = level->level + 1;
      level->items[level->num_items] = item;
      level->num_items = level->num_items+1;
    }
  }
  return item;
}

//! Destroy a hierarchy of ActionMenuLevels
//! @param root the root level in the hierarchy
//! @param each_cb a callback to call on every \ref ActionMenuItem in every level
//! @param context a context pointer to pass to each_cb on invocation
//! @note Typical implementations will cleanup memory allocated for the item label/data
//!       associated with each item in the callback
//! @note Hierarchy is traversed in post-order.
//!       In other words, all children items are freed before their parent is freed.
void action_menu_hierarchy_destroy(const ActionMenuLevel *root,
                                   ActionMenuEachItemCb each_cb,
                                   void *context){
  if(root) {
    for(uint16_t i=0; i<root->num_items; i++){
      ActionMenuItem* item = root->items[i];
      if(item->label) {
        free(item->label);
      }
      if(item->child) {
        action_menu_hierarchy_destroy(item->child, each_cb, context);
      }
      if(each_cb){
        each_cb(item, context);
      }
      free(item);
    }
    free(root->items);
    free((ActionMenuLevel *)root);
  }
}

//! Get the context pointer this ActionMenu was created with
//! @param action_menu A pointer to an ActionMenu
//! @return the context pointer initially provided in the \ref ActionMenuConfig.
//! NULL if none exists.
void *action_menu_get_context(ActionMenu *action_menu){
  return action_menu ? action_menu->config->context : NULL;
}

//! Get the root level of an ActionMenu
//! @param action_menu the ActionMenu you want to know about
//! @return a pointer to the root ActionMenuLevel for the given ActionMenu, NULL if invalid
ActionMenuLevel *action_menu_get_root_level(ActionMenu *action_menu){
  return action_menu ? (ActionMenuLevel *)action_menu->current_level : NULL;
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
  ActionMenu *menu = *((ActionMenu**)layer_get_data(layer));
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, menu->config->colors.background);
  graphics_fill_rect(ctx, bounds, 0, 0);

  graphics_context_set_fill_color(ctx, menu->config->colors.foreground);
  for(uint8_t i=0; i<menu->current_level->level; i++){
    graphics_fill_circle(ctx, (GPoint){bounds.size.w/2, 10 + i * 8}, 2);
  }
}

static uint16_t cb_get_num_rows(MenuLayer *ml, uint16_t i_section, void *ctx) {
  ActionMenu *menu = ctx;
  return menu->current_level->num_items;
}

static int16_t cb_get_cell_height(MenuLayer *ml, MenuIndex *i_cell, void *ctx) {
  ActionMenu *menu = ctx;

  GSize size = 
    graphics_text_layout_get_content_size( 
      menu->current_level->items[i_cell->row]->label, 
      fonts_get_system_font(ACTION_MENU_FONT), 
      GRect(0,0,144 - MENU_LAYER_OFFSET - 16,168), 
      GTextOverflowModeWordWrap, GTextAlignmentLeft);

  return size.h + 8 + 8;
}

static void cb_draw_row(GContext *g_ctx, const Layer *l_cell, MenuIndex *i_cell, void *ctx) {
  ActionMenu *menu = ctx;
  GRect bounds = layer_get_bounds(l_cell);

  if(menu_layer_get_selected_index(menu->menulayer).row == i_cell->row) {
    graphics_context_set_fill_color(g_ctx, GColorWhite);
    graphics_fill_rect(g_ctx, bounds, 0, GCornerNone);
  }

  bounds.origin.x += 4;
  bounds.size.w -= 2*4;

  graphics_context_set_fill_color(g_ctx, GColorBlack);
  graphics_fill_rect(g_ctx, bounds, 4, GCornersAll);

  bounds.size.w -= 2*4;
  bounds.origin.x += 4;

  bounds.origin.y += 4;
  bounds.size.h -= 2*4;

  graphics_draw_text(g_ctx,
    menu->current_level->items[i_cell->row]->label,
    fonts_get_system_font(ACTION_MENU_FONT),
    bounds,
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft,
    0);

  if(menu->current_level->items[i_cell->row]->child && menu_layer_get_selected_index(menu->menulayer).row == i_cell->row) {
    if(menu->arrow_image == NULL){
      menu->arrow_image = gbitmap_create_with_data(ARROW_IMAGE_DATA);
    }
    graphics_draw_bitmap_in_rect(g_ctx, menu->arrow_image, (GRect){.origin={116, bounds.origin.y + (bounds.size.h - 4) / 2},.size={7,5}});
  }
}

static void load_cb(Window *window) {
  ActionMenu *menu = window_get_user_data(window);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  menu->bg_layer = layer_create(bounds);
  layer_add_child(window_layer, menu->bg_layer);

  menu->column_layer = layer_create_with_data((GRect){.origin={0, 0}, .size={MENU_LAYER_OFFSET, bounds.size.h}}, sizeof(ActionMenu **));
  *((ActionMenu **)layer_get_data(menu->column_layer)) = menu;
  layer_set_update_proc(menu->column_layer, layer_update_proc);
  layer_add_child(menu->bg_layer, menu->column_layer);

  menu->menulayer = menu_layer_create((GRect){.origin={MENU_LAYER_OFFSET, 0}, .size={bounds.size.w - MENU_LAYER_OFFSET, bounds.size.h}});
  menu_layer_set_callbacks(menu->menulayer, menu, (MenuLayerCallbacks) {
    .get_num_rows       = cb_get_num_rows,
    .draw_row           = cb_draw_row,
    .get_cell_height    = cb_get_cell_height,
  });
  layer_add_child(menu->bg_layer, menu_layer_get_layer(menu->menulayer));
}

static void destroy_property_animation(PropertyAnimation **prop_animation) {
  if (*prop_animation == NULL) {
    return;
  }

  if (animation_is_scheduled((Animation*) *prop_animation)) {
    animation_unschedule((Animation*) *prop_animation);
  }

  property_animation_destroy(*prop_animation);
  *prop_animation = NULL;
}

static void disappear_cb(Window *window) {
  ActionMenu *menu = window_get_user_data(window);

  if(menu->config->will_close)
    menu->config->will_close(menu, menu->performed_action, menu->config->context);
}

static void unload_cb(Window *window) {
  ActionMenu *menu = window_get_user_data(window);

  if(menu->arrow_image)
    gbitmap_destroy(menu->arrow_image);

  destroy_property_animation(&menu->prop_animation);
  layer_destroy(menu->column_layer);
  layer_destroy(menu->bg_layer);
  menu_layer_destroy(menu->menulayer);
  window_destroy(window);

  if(menu->config->did_close)
    menu->config->did_close(menu, menu->performed_action, menu->config->context);

  free(menu->config);
  free(menu);
}

static void animate_menu(ActionMenu *menu);

static void animation_out_stopped(Animation *animation, bool finished, void *data) {
  ActionMenu *menu = data;

  menu->current_level = menu->tmp_level;
  menu->tmp_level = NULL;
  menu_layer_set_selected_index(menu->menulayer, (MenuIndex){0,0}, MenuRowAlignTop, false);
  menu_layer_reload_data(menu->menulayer);

  animate_menu(menu);
}

static void animate_menu(ActionMenu *menu) {
  Layer *layer = menu->bg_layer;
  GRect to_rect = layer_get_frame(layer);

  if(to_rect.origin.x == 0){
    to_rect.origin.x = -MENU_LAYER_OFFSET;
  }
  else {
    to_rect.origin.x = 0;
  }

  destroy_property_animation(&menu->prop_animation);

  menu->prop_animation = property_animation_create_layer_frame(layer, NULL, &to_rect);
  animation_set_duration((Animation*) menu->prop_animation, 150);
  animation_set_curve((Animation*) menu->prop_animation, AnimationCurveEaseInOut);
  animation_set_handlers((Animation*) menu->prop_animation, (AnimationHandlers) {.stopped = to_rect.origin.x ? animation_out_stopped : NULL}, menu);

  animation_schedule((Animation*) menu->prop_animation);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  ActionMenu *menu = context;

  if(menu->frozen)
    return;

  MenuLayer *ml = menu->menulayer;
  uint16_t row = menu_layer_get_selected_index(ml).row;
  if(menu->current_level->items[row]->child){
    menu->tmp_level = menu->current_level->items[row]->child;
    animate_menu(menu);
  }
  else if(menu->current_level->items[row]->cb) {
    menu->performed_action = menu->current_level->items[row];
    menu->performed_action->cb(
      menu,
      menu->performed_action,
      menu->config->context);

    if(menu->frozen)
      return;

    window_stack_remove(menu->window, true);
    if(menu->result_window) {
      window_stack_push(menu->result_window, true);
    }
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context)   {
  ActionMenu *menu = context;

  if(menu->frozen)
    return;

  menu_layer_set_selected_next(menu->menulayer, true, MenuRowAlignCenter, true);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  ActionMenu *menu = context;

  if(menu->frozen)
    return;

  menu_layer_set_selected_next(menu->menulayer, false, MenuRowAlignCenter, true);
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  ActionMenu *menu = context;

  if(menu->frozen)
    return;

  if(menu->current_level->parent) {
    menu->tmp_level = menu->current_level->parent;
    animate_menu(menu);
  }
  else {
    window_stack_remove(menu->window, true);
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

//! Open a new ActionMenu.
//! The ActionMenu acts much like a window. It fills the whole screen and handles clicks.
//! @param config the configuration info for this new ActionMenu
//! @return the new ActionMenu
ActionMenu *action_menu_open(ActionMenuConfig *config){
  ActionMenu *menu = NULL;
  if(config) {
    menu = malloc(sizeof(ActionMenu));
    if(menu) {
      memset(menu, 0, sizeof(ActionMenu));
      menu->config = malloc(sizeof(ActionMenuConfig));
      memcpy(menu->config, config, sizeof(ActionMenuConfig));

      menu->current_level = config->root_level;

      menu->window = window_create();
      window_set_user_data(menu->window, menu);
      window_set_window_handlers(menu->window, (WindowHandlers) {
        .load = load_cb,
        .disappear = disappear_cb,
        .unload = unload_cb,
      });
      window_set_click_config_provider_with_context(menu->window, click_config_provider, menu);
      window_set_background_color(menu->window, GColorBlack);
      window_set_fullscreen(menu->window, true);
      window_stack_push(menu->window, true);
    }
  }
  return menu;
}

//! Freeze the ActionMenu. The ActionMenu will no longer respond to user input.
//! @note this API should be used when waiting for asynchronous operation.
//! @param action_menu the ActionMenu
void action_menu_freeze(ActionMenu *action_menu){
  if(action_menu) {
    action_menu->frozen = true;
  }
}

//! Unfreeze the ActionMenu previously frozen with \ref action_menu_freeze
//! @param action_menu the ActionMenu to unfreeze
void action_menu_unfreeze(ActionMenu *action_menu){
  if(action_menu) {
    action_menu->frozen = false;
  }
}

//! Set the result window for an ActionMenu. The result window will be
//! shown when the ActionMenu closes
//! @param action_menu the ActionMenu
//! @param result_window the window to insert, pass NULL to remove the current result window
//! @note repeated call will result in only the last call to be applied, i.e. only
//! one result window is ever set
void action_menu_set_result_window(ActionMenu *action_menu, Window *result_window){
  action_menu ? action_menu->result_window = result_window : NULL;
}

//! Close the ActionMenu, whether it is frozen or not.
//! @note this API can be used on a frozen ActionMenu once the data required to
//! build the result window has been received and the result window has been set
//! @param action_menu the ActionMenu to close
//! @param animated whether or not show a close animation
void action_menu_close(ActionMenu *action_menu, bool animated){
  window_stack_remove(action_menu->window, true);
  if(action_menu->result_window) {
    window_stack_push(action_menu->result_window, true);
  }
}

#endif
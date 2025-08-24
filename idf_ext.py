import os
import shutil

print("idf_ext.py: Loading project extensions...")

def _clean_frontend_dist(project_path):
    """Remove the frontend dist directory if it exists"""
    front_dist_path = os.path.join(project_path, "front", "dist")
    if os.path.exists(front_dist_path):
        print(f"idf_ext.py: Removing frontend build directory: {front_dist_path}")
        try:
            shutil.rmtree(front_dist_path)
            print(f"idf_ext.py: Successfully removed {front_dist_path}")
        except OSError as e:
            print(f"idf_ext.py: Error removing {front_dist_path}: {e}")
    else:
        print(f"idf_ext.py: Frontend dist directory does not exist: {front_dist_path}")

def action_extensions(base_actions, project_path):
    """
    ESP-IDF extension hook. This function is called by the ESP-IDF build system.
    """
    if 'actions' in base_actions:
        actions_dict = base_actions['actions']

        if hasattr(actions_dict, 'keys') and 'fullclean' in actions_dict:
            original_fullclean_action = actions_dict['fullclean']
            original_callback = original_fullclean_action.get('callback')
            
            if original_callback:
                def extended_fullclean_callback(name, ctx, args, **kwargs):
                    _clean_frontend_dist(project_path)
                    return original_callback(name, ctx, args, **kwargs)
                
                # Update only the callback, preserve all other metadata
                original_fullclean_action['callback'] = extended_fullclean_callback
                print("idf_ext.py: Successfully extended 'fullclean' action callback")
            else:
                print("idf_ext.py: Could not find 'callback' in fullclean action")
        else:
            print("idf_ext.py: Could not find 'fullclean' in actions dictionary")
    return base_actions
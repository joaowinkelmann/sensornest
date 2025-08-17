import { Quasar } from 'quasar'
import { createApp } from 'vue'
import App from './App.vue'
import router from './router'

// Quasar styles and icons
import 'quasar/dist/quasar.css'
import '@quasar/extras/material-icons/material-icons.css'

import './styles/global.css'

createApp(App)
  .use(Quasar, { plugins: {} })
  .use(router)
  .mount('#app')

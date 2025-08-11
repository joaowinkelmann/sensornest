import './assets/main.css'
import 'leaflet/dist/leaflet.css' // 👈 Add this line
import 'leaflet-draw/dist/leaflet.draw.css' // 👈 Add this line

import { createApp } from 'vue'
import { createPinia } from 'pinia'

import App from './App.vue'
import router from './router'

const app = createApp(App)

app.use(createPinia())
app.use(router)

app.mount('#app')

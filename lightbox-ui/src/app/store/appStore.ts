import { Store } from "@tanstack/store"
import { useStore } from "@tanstack/react-store"
import type { Gallery } from "../lib/api/LightboxApi.ts"

type AppState = Partial<{
  brightness: number;
  matrix: string[][]
  gallery: Gallery
}> & {
  paintColor: string;
  brushSize: number;
}

export const appStore = new Store<AppState, (state: Partial<AppState>) => AppState>({
  paintColor: "#FF0000",
  brushSize: 1,
})

export function updateAppStore(state: Partial<AppState>) {
  return appStore.setState((s) => ({ ...s, ...state }))
}

export function useAppStore<TSelected = AppState>(
  selector?: (state: AppState) => TSelected,
): TSelected {
  return useStore(appStore, selector)
}

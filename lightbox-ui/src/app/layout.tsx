import { useState, useEffect } from "react"
import { NotFound } from "./pages/NotFound.tsx"
import { Main } from "./pages/Main/Main.tsx"
import "./app.css"

function useHashRoute() {
  const [route, setRoute] = useState(window.location.hash.slice(1) || "/")

  useEffect(() => {
    const onHashChange = () => setRoute(window.location.hash.slice(1) || "/")
    window.addEventListener("hashchange", onHashChange)
    return () => window.removeEventListener("hashchange", onHashChange)
  }, [])

  return route
}

export function Layout() {
  const route = useHashRoute()

  return <main className="w-dvw relative h-dvh">
    <div>
      {route === "/" && <Main/>}
      {route !== "/" && <NotFound/>}
    </div>
  </main>
}

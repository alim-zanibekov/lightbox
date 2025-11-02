import { StrictMode } from "react"
import { createRoot } from "react-dom/client"
import { Layout } from "./app/layout.tsx"
import { QueryClientProvider } from "@tanstack/react-query"
import { QueryClient } from "@tanstack/query-core"
const queryClient = new QueryClient()

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <QueryClientProvider client={queryClient}>
      <Layout/>
    </QueryClientProvider>
  </StrictMode>,
)

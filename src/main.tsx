import { createRoot } from "react-dom/client";
import App from "./App";
import { DEMO } from "./demo";
import { installMockHid } from "./dev/mockHid";
import { installFwHid } from "./dev/fwHid";
import "./styles.css";

if (new URLSearchParams(location.search).has("fw")) installFwHid(); else if (DEMO) installMockHid();

createRoot(document.getElementById("root")!).render(<App />);
